#pragma once
#include<iostream>
#include "Protocol.hpp"

class Task
{
public:
    Task() {}
    Task(int sock)
        : _sock(sock)
    {
    }

    //处理任务
    void ProcessOn()
    {
        _Handler(_sock);
    }

    ~Task() {}

private:
    int _sock;
    CallBack _Handler; // 回调
};
