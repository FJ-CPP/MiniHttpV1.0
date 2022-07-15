#pragma once
#include "Log.hpp"
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

const int BACKLOG = 5;

class TcpServer
{
private:
    static TcpServer *_svr;
    uint16_t _port;
    int _listenSock;


private:
    TcpServer(uint16_t port)
        : _port(port), _listenSock(-1)
    {
    }
    TcpServer(const TcpServer &o) = delete;
    TcpServer operator=(const TcpServer &o) = delete;

public:
    static TcpServer *GetInstance(uint16_t port)
    {
        static std::mutex mtx;
        if (_svr == nullptr)
        {
            mtx.lock();
            if (_svr == nullptr)
            {
                _svr = new TcpServer(port);
                _svr->InitServer();
            }
            mtx.unlock();
        }
        return _svr;
    }
    void InitServer()
    {
        Socket();
        Bind();
        Listen();
        LOG(INFO, "Init TcpServer success");
    }
    void Socket()
    {
        _listenSock = socket(AF_INET, SOCK_STREAM, 0);
        if (_listenSock == -1)
        {
            LOG(FATAL, "Create listen socket error!");
            exit(1);
        }
        // 设置地址复用(取消TIME_WAIT，方便立刻重新启用端口)
        int opt = 1;
        setsockopt(_listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        LOG(INFO, "Create listen socket success");
    }
    void Bind()
    {
        sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(_port);
        local.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(_listenSock, (sockaddr *)&local, sizeof(local)) == -1)
        {
            LOG(FATAL, "Bind listen socket error!");
            exit(2);
        }
        LOG(INFO, "Bind listen socket success");
    }
    void Listen()
    {
        if (listen(_listenSock, BACKLOG) == -1)
        {
            LOG(FATAL, "Listen error!");
            exit(3);
        }
        LOG(INFO, "Listen success");
    }
    int GetListenSocket()
    {
        return _listenSock;
    }
    ~TcpServer()
    {
        if (_listenSock >= 0)
        {
            close(_listenSock);
        }
    }
};

TcpServer *TcpServer::_svr = nullptr;
