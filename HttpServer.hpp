#pragma once
#include "TcpServer.hpp"
#include "ThreadPool.hpp"
#include <thread>
#include <signal.h>

// HTTP服务默认端口号
const uint16_t DEFAULT_PORT = 8080;

class HttpServer
{
private:
    bool _stop;
    int _port;

public:
    HttpServer(int port = DEFAULT_PORT)
        : _stop(false), _port(port)
    {
    }
    void InitHttpServer()
    { 
        // 忽略SIGPIPE信号，避免因为客户端在服务端写数据时关闭连接，从而导致服务器崩溃
        signal(SIGPIPE, SIG_IGN);
        LOG(INFO, "Init HttpServer success");
    }
    void Loop()
    {
        TcpServer *_tcp = TcpServer::GetInstance(_port);
        ThreadPool * threadPool = ThreadPool::GetInstance(5);
        int listenSock = _tcp->GetListenSocket();

        while (!_stop)
        {
            struct sockaddr_in peer;
            memset(&peer, 0, sizeof(peer));
            socklen_t len = sizeof(peer);

            int sock = accept(listenSock, (struct sockaddr *)&peer, &len);
            if (sock > 0)
            {
                // 为线程池推送本次请求
                Task t(sock);
                threadPool->PushTask(t);
            }
            else
            {
                std::cout << sock << std::endl;
                // accept执行出错，无视本次连接请求
                LOG(ERROR, "Accept Error!, error: " + std::string(strerror(errno)));
                continue;
            }
        }
    }
};
