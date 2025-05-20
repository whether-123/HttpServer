#pragma once

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>

// 工具类
class Util
{
public:
    // 读取完整的一行信息，并将\r\n or \r or \n 均换成\n结尾
    static int ReadLine(int sock, std::string &str)
    {
        char ch = 'W';
        // 读取完整的一行
        // 兼容各种行结束符 \r\n  \n  \r
        while (ch != '\n')
        {
            ssize_t s = recv(sock, &ch, 1, 0);
            if (s > 0)
            {
                if (ch == '\r')
                {
                    // \r\n或\r -> \n
                    // MSG_PEEK -- 查看数据，不取走
                    recv(sock, &ch, 1, MSG_PEEK);

                    if (ch == '\n')
                    {
                        recv(sock, &ch, 1, 0);
                    }
                    else
                    {
                        ch = '\n';
                    }
                }
                // 1.普通字符
                // 2.\n
                str.push_back(ch);
            }
            else if (s == 0) // 连接关闭
            {
                return 0;
            }
            else // 读取出错
            {
                return -1;
            }
        }
        return str.size();
    }
    
    //根据分割符sep，将目标子串拆成两个子串
    static bool Cutstr(const std::string& target,std::string& sub1,std::string& sub2,std::string sep)
    {
      size_t pos = target.find(sep);
      if(pos!=std::string::npos)
      {
        sub1=target.substr(0,pos);
        sub2=target.substr(pos+sep.size());
        return true;
      }
      return false;
    }
};