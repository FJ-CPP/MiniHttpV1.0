#include "HttpServer.hpp"
#include <memory>

void Usage(const char *str)
{
    printf("Usage:\n\t%s port\n", str);
}

int main(int argc, const char **argv)
{
    if (argc != 2)
    {
        Usage(argv[0]);
        return -1;
    }

    uint16_t port = atoi(argv[1]);

    std::shared_ptr<HttpServer> httpsvr(new HttpServer(port));
    httpsvr->InitHttpServer();
    httpsvr->Loop();

    return 0;
}