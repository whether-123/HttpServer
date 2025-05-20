#pragma once

#include<iostream>
#include<pthread.h>
#include<error.h>
#include<signal.h>
#include "TcpServer.hpp"
#include "Protocol.hpp"
#include "Log.hpp"
#include "Task.hpp"
#include "ThreadPool.hpp"

class HttpServer
{
public:
  HttpServer(int port = PORT) 
  : _port(port)
  , _stop(false)
  {}
  
  void InitServer()
  {
    //忽略SIGPIPE信号，防止写入时，客户端断开连接，出现崩溃
    signal(SIGPIPE,SIG_IGN);
  }

  void Loop()
  {
    LOG(INFO,"Loop begin");
    while(!_stop)
    {
      TcpServer* TcpSer = TcpServer::GetInstance(_port);
      struct sockaddr_in saddr;
      socklen_t len= sizeof(struct sockaddr_in);
      memset(&saddr,0,len);

      int sock=accept(TcpSer->Sock(),(struct sockaddr*)&saddr,&len);
      if(sock<0)
      {
        continue;
      }
      LOG(INFO,"Get a new Link");
      Task task(sock);
      ThreadPool::GetInstance()->PushTask(task);
    } 
  }
private:
  int _port;
  bool _stop;
};
