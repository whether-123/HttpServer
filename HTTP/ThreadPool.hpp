#pragma once

#include <iostream>
#include <queue>
#include <pthread.h>
#include "Task.hpp"

#define NUM 200

class ThreadPool
{
public:
    static ThreadPool* GetInstance()
    {
        pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
        if(Single_Instance == nullptr)
        {
            pthread_mutex_lock(&_mutex);
            if(Single_Instance == nullptr)
            {
                Single_Instance = new ThreadPool();
                Single_Instance->InitThreadPool();
            }
            pthread_mutex_unlock(&_mutex);  
        } 
        return Single_Instance;
    }

    bool TaskQueueIsEmpty()
    {
        return _TaskQueue.empty();
    }

    bool IsStop()
    {
        return _stop;
    }

    void ThreadWait()
    {
        pthread_cond_wait(&_cond, &_lock);
    }

    void ThreadWakeup()
    {
        pthread_cond_signal(&_cond);
    }

    void Lock()
    {
        pthread_mutex_lock(&_lock);
    }

    void UnLock()
    {
        pthread_mutex_unlock(&_lock);
    }

    static void *ThreadRoutine(void *args)
    {
        ThreadPool *tp = (ThreadPool *)args;

        while (true)
        {
            Task t;
            tp->Lock();
            while (tp->TaskQueueIsEmpty())
            {
                // 当线程被唤醒时，一定是持有锁的
                // 需要检测就绪队列是否为空
                tp->ThreadWait();
            }
            tp->PopTask(t);
            tp->UnLock();
            // 拿出任务后，让线程执行任务(对应套接字的任务)
            t.ProcessOn();
        }
    }

    bool InitThreadPool()
    {
        for (int i = 0; i < _num; i++)
        {
            pthread_t tid;
            if (pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
            {
                LOG(FATAL, "create ThreadPool error!");
                return false;
            }
        }
        LOG(INFO, "create ThreadPool success!");
        return true;
    }

    void PushTask(const Task &task)
    {
        //因为任务队列没有长度限制，有就可以往里面放
        //所以这里只设计了一个条件变量
        Lock();
        _TaskQueue.push(task);
        UnLock();
        ThreadWakeup();
    }

    void PopTask(Task &task)
    {
        task = _TaskQueue.front();
        _TaskQueue.pop();
    }

    ~ThreadPool()
    {
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_cond);
    }

private:
    ThreadPool(int num = NUM)
        : _num(num), _stop(false)
    {
        pthread_mutex_init(&_lock, nullptr);
        pthread_cond_init(&_cond, nullptr);
    }
    ThreadPool(const ThreadPool&) = delete;
    int _num;
    bool _stop;
    pthread_mutex_t _lock;
    pthread_cond_t _cond;
    std::queue<Task> _TaskQueue;
    static ThreadPool* Single_Instance;
};

ThreadPool* ThreadPool::Single_Instance = nullptr;