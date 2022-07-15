#pragma once
#include "Task.hpp"
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>

const int NUM = 5;

class ThreadPool
{
private:
    static ThreadPool *_instance;
    std::queue<Task> _taskQueue;
    std::mutex _mtx;
    std::condition_variable _condition;
    int _num;   // 线程数量
    bool _stop; // 线程池是否停止运行

private:
    ThreadPool(int num = NUM)
        : _num(num), _stop(false)
    {
    }

    static void Routine(ThreadPool *threadPool)
    {
        while (true)
        {
            std::unique_lock<std::mutex> ulock(threadPool->_mtx);
            while (threadPool->_taskQueue.empty()) // 防止伪唤醒
            {
                threadPool->_condition.wait(ulock);
            }
            Task t;
            threadPool->PopTask(t);
            ulock.unlock();
            t.Execute();
        }
    }
    void PopTask(Task &t)
    {
        t = _taskQueue.front();
        _taskQueue.pop();
    }
    ThreadPool(const ThreadPool &o) = delete;
    ThreadPool &operator=(const ThreadPool &o) = delete;

public:
    static ThreadPool *GetInstance(int num = NUM)
    {
        static std::mutex mtx;
        if (_instance == nullptr)
        {
            mtx.lock();
            if (_instance == nullptr)
            {
                _instance = new ThreadPool(num);
                _instance->InitThreadPool();
                LOG(INFO, "Init ThreadPool success");
            }
            mtx.unlock();
        }
        return _instance;
    }
    void InitThreadPool()
    {
        for (int i = 0; i < _num; ++i)
        {
            std::thread(Routine, _instance).detach();
        }
    }
    void PushTask(const Task &t)
    {
        //_mtx.lock();
        _taskQueue.push(t);
        //_mtx.unlock();
        _condition.notify_one();
    }
};

ThreadPool *ThreadPool::_instance = nullptr;