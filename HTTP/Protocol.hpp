#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include "Util.hpp"
#include "Log.hpp"

#define SEP ": "
#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"
#define PAGE_404 "404.html"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define SERVER_ERROR 500

static std::string StatusCodeDesc(int code)
{
  std::string desc;
  switch (code)
  {
  case 200:
    desc = "OK";
    break;
  case 404:
    desc = "Not Found";
    break;
  default:
    break;
  }
  return desc;
}

static std::string SuffixtoDesc(const std::string &suffix)
{
  static std::unordered_map<std::string, std::string> suffixtoDesc = {
      {".html", "text/html"},
      {".css", "text/css"},
      {".js", "application/javascript"},
      {".jpg", "application/x-jpg"},
      {".xml", "application/xml"},
      {".pdf","application/pdf"},
      {".png","image/png"},

  };

  auto iter = suffixtoDesc.find(suffix);
  if (iter != suffixtoDesc.end())
  {
    return iter->second;
  }
  return "text/html";
}

// http请求
class HttpRequest
{
public:
  std::string request_line;                // 请求行
  std::vector<std::string> request_header; // 请求报头
  std::string blank;                       // 正文与报头之间的空行
  std::string request_body;                // 请求正文

  // 解析请求行
  std::string method;
  std::string uri;
  std::string version;

  // 解析请求报头
  std::unordered_map<std::string, std::string> header_kv;
  int content_length;
  std::string path;         // 访问的路径
  std::string suffix;       // 请求资源的后缀
  std::string query_String; // 路径后面的参数

  // 如果用户上传了数据，Http的任务就是将数据拿上来，
  // 处理数据本身和Http的关系并不大
  // Http需要通过CGI机制，来处理数据
  // 调用目标程序，传递目标数据，拿到目标结果
  bool cgi;

public:
  HttpRequest() : content_length(0), cgi(false) {}
  ~HttpRequest() {}
};

// http响应
class HttpResponse
{
public:
  std::string status_line;                  // 状态行
  std::vector<std::string> response_header; // 响应报头
  std::string blank;                        // 正文与报头之间的空行
  std::string response_body;                // 响应正文

  int status_code;
  int fd;   // 要访问资源的文件描述符
  int size; // 要访问的资源的大小
public:
  HttpResponse() : blank(LINE_END), status_code(OK) {}
  ~HttpResponse() {}
};

// 完成对应的业务逻辑
// 读取请求，分析请求，构建响应，发送响应
class EndPoint
{
public:
  EndPoint(int sock)
      : _sock(sock)
      , _stop(false)
  {}

  // 读取和分析请求
  void RcvHttpRequest()
  {
    // 读取请求行和请求报头
    if ((!RecvHttpRequestLine()) && (!RecvHttpRequestHeader()))
    {
      // 读取没有出错
      ParseHttpRequestLine();   // 解析请求行
      ParseHttpRequestHeader(); // 解析请求报头
      RecvHttpRequesBody();     // 读取正文
    }
  }

  // 构建响应
  // 什么时候，需要使用CGI来进行数据处理呢？只要用户有数据上传上来
  void BuildHttpResponse()
  {
    // 请求已经全部读完，可以直接构建响应
    auto &code = _HttpRes.status_code;
    std::string temp_path;
    struct stat st;
    int size = 0;
    size_t found = 0;
    if (_HttpReq.method != "POST" && _HttpReq.method != "GET")
    {
      LOG(WARNING, "method is not right");
      code = BAD_REQUEST;
      goto END;
    }

    // URL XX.XX.XX.XX:8080/a/b/c?x=1&y=2
    // 变量之间用&隔开，路径和数据之间用？隔开
    if (_HttpReq.method == "GET")
    {
      // 如果是GET方法，需要判断是否带参
      ssize_t pos = _HttpReq.uri.find('?');
      if (pos != std::string::npos)
      {
        Util::Cutstr(_HttpReq.uri, _HttpReq.path, _HttpReq.query_String, "?");
        _HttpReq.cgi = true;
      }
      else
      {
        // 没有参数，路径就是URL
        _HttpReq.path = _HttpReq.uri;
      }
    }
    else if (_HttpReq.method == "POST")
    {
      // POST方法一定带参，且在请求正文中
      _HttpReq.cgi = true;
      _HttpReq.path = _HttpReq.uri;
    }
    else
    {}

    // 拼接web根目录
    temp_path = _HttpReq.path;
    _HttpReq.path = WEB_ROOT;
    _HttpReq.path += temp_path;

    if (_HttpReq.path[_HttpReq.path.size() - 1] == '/')
    {
      _HttpReq.path += HOME_PAGE;
    }

    // stat函数，打开对应路径下的文件，成功返回0，失败返回-1(文件不存在)
    if (stat(_HttpReq.path.c_str(), &st) == 0)
    {
      // 请求的是一个目录，不能直接返回该目录
      // 返回该目录下的默认首页
      if (S_ISDIR(st.st_mode))
      {
        // 虽然是一个目录，但是绝对不会以/结尾
        // 以/结尾的上面已经特殊处理
        _HttpReq.path += "/";
        _HttpReq.path += HOME_PAGE;
        // 重新获取一下路径下的文件
        stat(_HttpReq.path.c_str(), &st);
      }

      // 请求的是一个可执行程序
      // 查看该文件下，拥有者，所属组，其他人是否有执行权限
      if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
      {
        // 如果请求的资源是可执行程序，也需要CGI
        _HttpReq.cgi = true;
      }
      _HttpRes.size = st.st_size;
    }
    else
    {
      // 说明资源是不存在的
      LOG(WARNING, _HttpReq.path + " Not Found!");
      code = NOT_FOUND;
      goto END;
    }

    found = _HttpReq.path.rfind(".");
    if (found == std::string::npos)
    {
      _HttpReq.suffix = ".html";
    }
    else
    {
      _HttpReq.suffix = _HttpReq.path.substr(found);
    }

    if (_HttpReq.cgi) // 说明有上传数据，需要CGI机制去处理
    {
      // 执行目标程序，返回结果到_HttpRes.response_body中
      code = ProcessCgi();
    }
    else // 说明没有上传数据，仅需要网页返回
    {
      // 1.目标网页一定是存在的
      // 2.返回并不是简单的返回网页，而是要构建Http响应
      code = ProcessNonCgi();
    }

  END:
    // 填充状态行，响应报头，空行，正文
    BuildHttpResponseHelper();
  }

  void SendHttpResponse()
  {
    send(_sock, _HttpRes.status_line.c_str(), _HttpRes.status_line.size(), 0);
    for (auto it : _HttpRes.response_header)
    {
      send(_sock, it.c_str(), it.size(), 0);
    }
    send(_sock, _HttpRes.blank.c_str(), _HttpRes.blank.size(), 0);
    // 静态网页 -> fd中
    // cgi  -> 响应正文中
    if (_HttpReq.cgi)
    {
      auto &response_body = _HttpRes.response_body;
      size_t size = 0;
      size_t total = 0;
      const char *start = response_body.c_str();
      while (total < response_body.size() && (size = send(_sock, start + total, response_body.size() - total, 0)) > 0)
      {
        total += size;
      }
    }
    else
    {
      sendfile(_sock, _HttpRes.fd, nullptr, _HttpRes.size);
      close(_HttpRes.fd);
    }
  }

  bool IsStop()
  {
    return _stop;
  }
  ~EndPoint()
  {
    close(_sock);
  }

private:
  void HandlerError(std::string page)
  {
    _HttpReq.cgi = false;
    // 要给用户返回对应的404页面
    _HttpRes.fd = open(page.c_str(), O_RDONLY);
    // 打开404页面后，构建响应报头
    if (_HttpRes.fd > 0)
    {
      struct stat st;
      stat(page.c_str(), &st);
      _HttpRes.size = st.st_size;

      std::string line = "Content-Type: text/html";
      line += LINE_END;
      _HttpRes.response_header.push_back(line);

      line = "Content-Length: ";
      line += std::to_string(st.st_size);
      line += LINE_END;
      _HttpRes.response_header.push_back(line);
    }
  }

  void BuildOKResponse()
  {
    std::string line = "Content-Type: ";
    line += SuffixtoDesc(_HttpReq.suffix);
    line += LINE_END;
    _HttpRes.response_header.push_back(line);

    line = "Content-Length: ";
    if (_HttpReq.cgi)
    {
      line += std::to_string(_HttpRes.response_body.size());
    }
    else
    {
      line += std::to_string(_HttpRes.size);
    }
    line += LINE_END;
    _HttpRes.response_header.push_back(line);
  }
  // 出错的时候
  void BuildHttpResponseHelper()
  {
    auto &code = _HttpRes.status_code;
    auto &status_line = _HttpRes.status_line;
    status_line += HTTP_VERSION;
    status_line += " ";
    status_line += std::to_string(code);
    status_line += " ";
    status_line += StatusCodeDesc(code);
    status_line += LINE_END;

    // 构建响应正文和报头
    std::string path = WEB_ROOT;
    path += "/";
    switch (code)
    {
    case OK:
      BuildOKResponse();
      break;
    case NOT_FOUND:
      path += PAGE_404;
      HandlerError(path);
      break;
    case BAD_REQUEST:
      path += PAGE_404;
      HandlerError(path);
      break;
    case SERVER_ERROR:
      path += PAGE_404;
      HandlerError(path);
      break;
    // case 502:
    //   HandlerError(PAGE_502);
    default:
      break;
    }
  }

  bool RecvHttpRequestLine()
  {
    auto &line = _HttpReq.request_line;
    // 读取完整的一行信息，并将\r\n or \r or \n 均换成\n结尾
    if (Util::ReadLine(_sock, line) > 0)
    {
      // 将结尾的/n去掉
      line.resize(line.size() - 1);
      LOG(INFO, _HttpReq.request_line);
    }
    // 读取出错时
    else
    {
      _stop = true;
    }
    return _stop;
  }

  bool RecvHttpRequestHeader()
  {
    std::string line;
    while (true)
    {
      line.clear();
      // 读取完整的一行信息
      if (Util::ReadLine(_sock, line) <= 0)
      {
        // 如果读取出错，直接跳出循环
        _stop = true;
        break;
      }
      // 代表报头已经读取完毕
      if (line == "\n")
      {
        _HttpReq.blank = line;
        break;
      }
      // 去掉结尾的\n
      line.resize(line.size() - 1);
      _HttpReq.request_header.push_back(line);
      LOG(INFO, line);
    }
    return _stop;
  }

  void ParseHttpRequestLine()
  {
    auto &line = _HttpReq.request_line;
    std::stringstream ss(line);
    ss >> _HttpReq.method >> _HttpReq.uri >> _HttpReq.version;
    auto &method = _HttpReq.method;
    // 将方法转成大写 例 Get -> GET
    std::transform(method.begin(), method.end(), method.begin(), ::toupper);
  }

  void ParseHttpRequestHeader()
  {
    std::string key;
    std::string value;
    // 解析成<K,V>
    for (auto &it : _HttpReq.request_header)
    {
      if (Util::Cutstr(it, key, value, SEP))
      {
        _HttpReq.header_kv.insert(make_pair(key, value));
      }
    }
  }

  bool IsNeedRecvHttpRequesBody()
  {
    auto &method = _HttpReq.method;
    if (method == "POST")
    {
      auto &header_kv = _HttpReq.header_kv;
      auto it = header_kv.find("Content-Length");
      if (it != header_kv.end())
      {
        LOG(INFO, "Post Method,Content-Length: " + it->second);
        _HttpReq.content_length = atoi(it->second.c_str());
        return true;
      }
    }
    return false;
  }

  bool RecvHttpRequesBody()
  {
    if (IsNeedRecvHttpRequesBody())
    {
      int content_length = _HttpReq.content_length;
      auto &body = _HttpReq.request_body;

      char ch = 0;
      while (content_length)
      {
        ssize_t s = recv(_sock, &ch, 1, 0);
        if (s > 0)
        {
          body.push_back(ch);
          content_length--;
        }
        else
        {
          _stop = true;
          break;
        }
      }
      LOG(INFO, body);
    }
    return _stop;
  }

  int ProcessNonCgi()
  {
    _HttpRes.fd = open(_HttpReq.path.c_str(), O_RDONLY);
    if (_HttpRes.fd >= 0)
    {
      return OK;
    }
    return NOT_FOUND;
  }

  int ProcessCgi()
  {
    int code = OK;
    // 父进程的数据
    auto &query_string = _HttpReq.query_String; // GET
    auto &body_text = _HttpReq.request_body; // POST
    auto &method = _HttpReq.method;
    auto &path = _HttpReq.path; // 子进程执行的目标程序，一定存在
    int content_length = _HttpReq.content_length;
    auto &response_body = _HttpRes.response_body;

    // GET方法的参数用环境变量获取
    std::string query_string_env;
    std::string method_env;
    std::string content_length_env;

    // POST方法的参数用匿名管道获取
    // 父进程写入初始数据，子进程读取
    int output[2];
    // 子进程写入处理好的数据，父进程读取
    int input[2];
    // 匿名管道 -- 在内存中创建一个缓冲区
    // 挂接上两个fd，一个读，一个写
    if (pipe(input) < 0)
    {
      LOG(ERROR, "pipe input error");
      code = SERVER_ERROR;
      return code;
    }

    if (pipe(output) < 0)
    {
      LOG(ERROR, "pipe output error");
      code = SERVER_ERROR;
      return code;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
      close(output[1]);
      close(input[0]);

      method_env = "METHOD=";
      method_env += method;
      putenv((char *)method_env.c_str());

      if (method == "GET")
      {
        query_string_env = "QUERY_STRING=";
        query_string_env += query_string;
        putenv((char *)query_string_env.c_str());
        LOG(INFO, "GET MEthod, ADD QUERY Env");
      }
      else if (method == "POST")
      {
        content_length_env = "CONTENT_LENGTH=";
        content_length_env += std::to_string(content_length);
        putenv((char *)content_length_env.c_str());
        LOG(INFO, "POST MEthod, ADD CONTENT Env");
      }
      else
      {}

      // exec
      // 替换成功之后，目标子进程如何得知曾经打开的pipe文件？
      // 1.程序替换，只替换代码和数据，并不替换内核进程相关的数据结构(包括--文件描述符表)
      // 2.约定--让目标被替换后的进程，读取管道等价于读取标准输入，写入管道，等价于写到标准输出
      // 3.在exec执行前，进行重定向
      dup2(output[0], 0);
      dup2(input[1], 1);

      execl(path.c_str(), path.c_str(), nullptr);
      exit(1);
    }
    else if (pid < 0)
    {
      LOG(ERROR, "fork error!");
      return SERVER_ERROR;
    }
    else
    {
      close(output[0]);
      close(input[1]);

      if (method == "POST")
      {
        const char *start = body_text.c_str();
        int total = 0;
        int size = 0;
        // 直到把数据写完，或者写失败
        while (total < content_length && (size = write(output[1], start + total, body_text.size() - total)) > 0)
        {
          total += size;
        }
      }

      char ch = 0;
      while (read(input[0], &ch, 1))
      {
        response_body.push_back(ch);
      }

      int status = 0;
      pid_t ret = waitpid(pid, &status, 0);
      if (ret == pid) // 成功等待
      {
        if (WIFEXITED(status)) // 成功退出
        {
          if (WEXITSTATUS(status) == 0) // 且退出码没有问题
          {
            code = OK;
          }
          else
          {
            code = BAD_REQUEST;
          }
        }
        else
        {
          code = SERVER_ERROR;
        }
      }
    }
    return code;
  }

private:
  int _sock;
  bool _stop;
  HttpRequest _HttpReq;
  HttpResponse _HttpRes;
};

class CallBack
{
public:
  CallBack() {}

  void operator()(int sock)
  {
    HandlerRequest(sock);
  }
  void HandlerRequest(int sock)
  {
    LOG(INFO, "Hander Request Begin");
    std::cout << "get a new link ...." << sock << std::endl;

    EndPoint *ep = new EndPoint(sock);
    ep->RcvHttpRequest();
    if (!ep->IsStop())
    {
      LOG(INFO, "Recv No Error, Begin Build And Send");
      ep->BuildHttpResponse();
      ep->SendHttpResponse();
    }
    else
    {
      LOG(WARNING, "Recv Error, Stop Build And Send");
    }
    delete ep;

    LOG(INFO, "Hander Request End");
  }

  ~CallBack() {}
};