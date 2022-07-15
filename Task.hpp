#pragma once
#include "Protocol.hpp"

class Task
{
private:
    int _sock;
    Callback _handler;
public:
    Task(int sock = -1)
        : _sock(sock)
    {
    }
    void Execute()
    {
        _handler(_sock);
    }
};