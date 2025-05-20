#pragma once

#include <iostream>
#include <cstdlib>
#include <cstring>
#include<unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "Log.hpp"

const static int PORT = 8081;
const static int BACKLOG = 5;

class TcpServer
{
public:
    //饿汉模式
    static TcpServer *GetInstance(int port)
    {
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        if (_TcpSver == nullptr)
        {
            pthread_mutex_lock(&lock);
            if (_TcpSver == nullptr)
            {
                _TcpSver = new TcpServer(port);
                _TcpSver->InitServer();
            }
            pthread_mutex_unlock(&lock);
        }
        return _TcpSver;
    }

    void InitServer()
    {
        Socket();
        Bind();
        Listen();
        LOG(INFO,"TCP_Server init ... success!");
    }

    void Socket()
    {
        _listenSock = socket(AF_INET, SOCK_STREAM, 0);
        if (_listenSock < 0)
        {
            LOG(FATAL,"soket error!");
            exit(1);
        }
        
        int opt=1;
        //设置地址复用，当服务器挂掉了，可以立即重启
        setsockopt(_listenSock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
        LOG(INFO,"create socket ... success!");
    }

    void Bind()
    {
        struct sockaddr_in saddr;
        memset(&saddr,0,sizeof(saddr));
        saddr.sin_family=AF_INET;
        saddr.sin_port=htons(_port);
        //云服务器不能直接绑定公网ip
        const char* ip="0.0.0.0";
        inet_pton(AF_INET,ip,&saddr.sin_addr);
        //saddr.sin_addr.s_addr=INADDR_ANY;

        if(bind(_listenSock,(struct sockaddr*)&saddr,sizeof(saddr))<0)
        {
            LOG(FATAL,"bind error!");
            exit(2);
        }
        LOG(INFO,"bind socket ... success!");
    }

    void Listen()
    {
        if(listen(_listenSock,BACKLOG))
        {
            LOG(FATAL,"listen socket error!");
            exit(3);
        }
        LOG(INFO,"listen socket ... success!");
    }
    
    int Sock()
    {
        return _listenSock;
    }

    ~TcpServer()
    {
        if(_listenSock>=0)
        {
            close(_listenSock);
        }
    }

private:
    int _port;     //端口号
    int _listenSock;  //监听套接字
    static TcpServer *_TcpSver;  

    TcpServer(int port = PORT)
        : _port(port), _listenSock(-1)
    {}

    TcpServer(const TcpServer &) {}
};

TcpServer *TcpServer::_TcpSver = nullptr;