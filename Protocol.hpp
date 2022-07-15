#pragma once
#include "Util.hpp"
#include "Log.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

const std::string WEB_ROOT = "wwwroot";
const std::string HOME_PAGE = "index.html";
const std::string NOT_FOUND_PAGE = "404.html";

const std::string HTTP_VERSION = "HTTP/1.0";
const std::string END_LINE = "\r\n";

enum StatusCode
{
    OK = 200,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    INTERNAL_SERVER_ERROR = 500
};

static std::string Code2Desc(int code)
{
    std::unordered_map<int, std::string> desc = {
        {OK, "OK"},
        {BAD_REQUEST, "Bad Request"},
        {NOT_FOUND, "Not Found"},
        {INTERNAL_SERVER_ERROR, "Internal Server Error"}};
    if (desc.find(code) != desc.end())
    {
        return desc[code];
    }
    return "OK";
}

static std::string Suffix2Type(std::string suffix)
{
    std::unordered_map<std::string, std::string> type = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".xml", "application/xml"},
        {".jpg", "application/x-jpg"}};
    if (type.find(suffix) != type.end())
    {
        return type[suffix];
    }
    return "text/html";
}

class HttpRequest
{
public:
    // 解析前的报文内容
    std::string _requestLine;                // 请求行
    std::vector<std::string> _requestHeader; // 请求报头
    std::string _blank;                      // 空行
    std::string _requestBody;                // 请求正文

    // 解析后的报文内容
    std::string _method;                                  // 请求方法
    std::string _uri;                                     // 请求uri
    std::string _version;                                 // http版本
    std::unordered_map<std::string, std::string> _header; // 请求报头
    int _contentLength;                                   // 正文长度

    // uri的解析结果
    std::string _path;  // 资源路径
    std::string _query; // 请求参数

    bool _cgi; // 该请求是否需要cgi处理
public:
    HttpRequest()
        : _cgi(false)
    {
    }
};

class HttpResponse
{
public:
    std::string _statusLine;                  // 状态行
    std::vector<std::string> _responseHeader; // 响应报头
    std::string _blank;                       // 空行
    std::string _responseBody;                // 响应正文

    int _statusCode;       // 状态码
    size_t _fd;            // 请求访问的目标文件fd
    std::string _suffix;   // 请求访问的目标文件后缀
    size_t _contentLength; // 响应正文的长度
public:
    HttpResponse()
        : _blank(END_LINE), _statusCode(OK), _fd(-1)
    {
    }
    ~HttpResponse()
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
    }
};

class EndPoint
{
private:
    int _sock;
    HttpRequest _httpRequest;
    HttpResponse _httpResponse;
    bool _stop;

private:
    bool RecvHttpRequestLine()
    {
        auto &line = _httpRequest._requestLine;
        if (Util::GetLine(_sock, line) > 0)
        {
            line.pop_back(); // 去掉末尾的'\n'
            LOG(INFO, line);
        }
        else
        {
            _stop = true; // 读取失败，终止本次请求
        }
        return _stop;
    }
    bool RecvHttpRequestHeader()
    {
        std::string line;

        // 按行读取请求报头，遇到'\n'停止
        while (1)
        {
            if (Util::GetLine(_sock, line) > 0)
            {
                if (line == "\n")
                {
                    break;
                }
                line.pop_back(); // 去掉行末'\n'
                _httpRequest._requestHeader.emplace_back(line);
                line.clear();
            }
            else
            {
                _stop = true; // 读取失败，终止本次请求
                break;
            }
        }

        if (line == "\n")
        {
            _httpRequest._blank = "\n";
        }
        return _stop;
    }
    bool NeedToRecvHttpRequestBody()
    {
        if (_httpRequest._method == "POST")
        {
            if (_httpRequest._header.find("Content-Length") != _httpRequest._header.end())
            {
                _httpRequest._contentLength = stoi(_httpRequest._header["Content-Length"]);
                return true;
            }
        }
        return false;
    }
    void RecvHttpRequestBody()
    {
        if (NeedToRecvHttpRequestBody())
        {
            char ch;
            int n = _httpRequest._contentLength;
            while (n--)
            {
                ssize_t s = recv(_sock, &ch, 1, 0);
                if (s > 0)
                {
                    _httpRequest._requestBody.push_back(ch);
                }
            }
        }
    }

private:
    void ParseHttpRequestLine()
    {
        auto &line = _httpRequest._requestLine;
        std::stringstream ss(line);
        ss >> _httpRequest._method >> _httpRequest._uri >> _httpRequest._version;
        // 将请求方法统一转为大写
        auto &method = _httpRequest._method;
        std::transform(method.begin(), method.end(), method.begin(), ::toupper);
    }
    void ParseHttpRequestHeader()
    {
        std::string key;
        std::string value;

        for (auto &line : _httpRequest._requestHeader)
        {
            // 请求报头格式为key: value
            Util::CutString(line, ": ", key, value);
            _httpRequest._header[key] = value;
        }
    }

private:
    int ProcessNonCgi()
    {
        // 对于非cgi的请求，只需打开目标文件即可
        _httpResponse._fd = open(_httpRequest._path.c_str(), O_RDONLY);
        if (_httpResponse._fd >= 0)
        {
            return OK;
        }
        return NOT_FOUND;
    }
    int ProcessCgi()
    {
        int retCode = OK;
        // 将请求方法写入环境变量，方便子进程获取
        std::string methodEnv = "METHOD=" + _httpRequest._method;
        std::string queryEnv = "QUERY=" + _httpRequest._query;
        putenv((char *)methodEnv.c_str());
        // GET方法的请求参数在uri中，而uri限定为2048字节，因此用环境变量传递更为高效
        if (_httpRequest._method == "GET")
        {
            queryEnv = "QUERY=" + _httpRequest._query;
            putenv((char *)queryEnv.c_str()); // putenv需要注意字符串的生命周期！！！
        }
        else if (_httpRequest._method == "POST")
        {
            // 如果是POST方法，则将正文长度以环境变量的方式传给子进程
            queryEnv = "CONTENT_LENGTH=" + std::to_string(_httpRequest._contentLength);
            putenv((char *)queryEnv.c_str());
        }
        int input[2];  // 父进程读取管道
        int output[2]; // 父进程写入管道

        if (pipe(input) == -1 || pipe(output) == -1)
        {
            LOG(ERROR, "Pipe error!");
            return INTERNAL_SERVER_ERROR;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            // 子进程往output[0]写，从intput[1]读，其余管道读写端关闭
            close(input[0]);
            close(output[1]);
            // 为了避免程序替换后子进程不知道管道读写端描述符，因此约定：
            // fd=0用来读父进程传来的数据，fd=1用来向父进程写数据
            dup2(output[0], 0);
            dup2(input[1], 1);

            std::cerr << "_httpRequest._query : " << getenv("QUERY") << std::endl;
            execl(_httpRequest._path.c_str(), _httpRequest._path.c_str(), nullptr);
            LOG(ERROR, "Execl error: " + std::string(strerror(errno)));
            exit(0);
        }
        else if (pid > 0)
        {
            // 父进程从input[0]读，往output[1]写，其余管道读写端关闭
            close(input[1]);
            close(output[0]);
            // POST方法的请求参数在报文体中，可能较长，因此需要用管道向子进程传递请求参数
            if (_httpRequest._method == "POST")
            {
                ssize_t s = 0;
                ssize_t total = 0;
                auto &body = _httpRequest._requestBody;
                const char *start = body.c_str();
                // 避免管道满导致一次写不完所有数据，因此循环写入数据
                while (total < body.size() && (s = write(output[1], start + total, body.size() - total)) > 0)
                {
                    total += s;
                }
            }

            // 从子进程获取响应正文
            char ch = 0;
            while (read(input[0], &ch, 1) > 0)
            {
                _httpResponse._responseBody.push_back(ch);
            }
            _httpResponse._contentLength = _httpResponse._responseBody.size();

            int status = 0;
            waitpid(pid, &status, 0); // 阻塞等待子进程退出
            if (WIFEXITED(status))
            {
                if (WEXITSTATUS(status) == 0)
                {
                    // 子进程成功执行
                    retCode = OK;
                }
                else
                {
                    retCode = BAD_REQUEST;
                }
            }
            else
            {
                LOG(ERROR, "WIFEXITED failed, sig:" + std::to_string(WTERMSIG(status)));
                retCode = NOT_FOUND;
            }
            // 线程结束不会关闭fd，所以必须手动关闭管道文件，避免套接字泄露
            close(input[0]);
            close(output[1]);
        }
        else
        {
            LOG(ERROR, "Fork error!");
            return INTERNAL_SERVER_ERROR;
        }
        return retCode;
    }
    void BuildHttpResponseHelper()
    {
        // 构建状态行
        _httpResponse._statusLine += HTTP_VERSION; // HTTP版本
        _httpResponse._statusLine += " ";
        _httpResponse._statusLine += std::to_string(_httpResponse._statusCode); // 状态码
        _httpResponse._statusLine += " ";
        _httpResponse._statusLine += Code2Desc(_httpResponse._statusCode); // 状态码描述
        _httpResponse._statusLine += _httpResponse._blank;

        // 根据状态码构建响应报头和响应正文
        switch (_httpResponse._statusCode)
        {
        case OK:
            BuildOkResponse();
            break;
        // case BAD_REQUEST:
        //     HandleError(BAD_REQUEST_PAGE);
        //     break;
        case NOT_FOUND:
            BuildErrorResponse(NOT_FOUND_PAGE);
        default:
            break;
        }
    }
    void BuildOkResponse()
    {
        // 响应报头
        std::string line;
        line = "Content-Length: " + std::to_string(_httpResponse._contentLength) + _httpResponse._blank;
        _httpResponse._responseHeader.emplace_back(line);
        line = "Content-Type: " + Suffix2Type(_httpResponse._suffix) + _httpResponse._blank;
        _httpResponse._responseHeader.emplace_back(line);
    }
    void BuildErrorResponse(std::string page)
    {
        std::string line;
        page = WEB_ROOT + "/" + page; // 访问web根目录下的错误页面
        _httpResponse._fd = open(page.c_str(), O_RDONLY);
        if (_httpResponse._fd > 0)
        {
            struct stat st;
            stat(page.c_str(), &st);
            _httpResponse._contentLength = st.st_size;
            line = "Content-Type: text/html" + END_LINE;
            _httpResponse._responseHeader.emplace_back(line);
            line = "Content-Length: " + std::to_string(st.st_size) + END_LINE;
            _httpResponse._responseHeader.emplace_back(line);
        }
        _httpRequest._cgi = false; // 下一步发送普通文件，而非cgi的处理结果
    }

public:
    EndPoint(int sock)
        : _sock(sock), _stop(false)
    {
    }
    ~EndPoint()
    {
        if (_sock >= 0)
        {
            close(_sock);
        }
    }
    bool IsStop()
    {
        return _stop;
    }
    void RecvHttpRequest()
    {
        // 获取请求报文
        if (RecvHttpRequestLine() == false && RecvHttpRequestHeader() == false)
        {
            // 解析请求报文
            ParseHttpRequestLine();
            ParseHttpRequestHeader();
            // 根据解析结果，获取请求正文
            RecvHttpRequestBody();
        }
    }
    void BuildHttpResponse()
    {
        // 获取请求目标文件的路径
        if (_httpRequest._method == "GET")
        {
            // 解析请求uri：如果GET方法有请求参数，则请求路径和请求参数会以'?'作为分隔
            if (_httpRequest._uri.find('?') != std::string::npos)
            {
                Util::CutString(_httpRequest._uri, "?", _httpRequest._path, _httpRequest._query);
                _httpRequest._cgi = true; // 有请求参数，需要cgi介入
            }
            else
            {
                _httpRequest._path = _httpRequest._uri;
            }
        }
        else if (_httpRequest._method == "POST")
        {
            _httpRequest._path = _httpRequest._uri;
            _httpRequest._cgi = true; // 有请求参数，需要cgi介入
        }
        else
        {
            // 仅处理GET和POST方法
            LOG(WARNING, "Method not found!");
            _httpResponse._statusCode = BAD_REQUEST;
            goto END;
        }
        // 以WEB_ROOT为根目录搜索路径
        _httpRequest._path = WEB_ROOT + _httpRequest._path;
        // 路径以'/'结尾，说明该路径可能是目录，且无指定文件，默认返回目录主页
        if (_httpRequest._path[_httpRequest._path.size() - 1] == '/')
        {
            _httpRequest._path += HOME_PAGE;
        }
        // 根据路径查看目标文件的属性信息
        struct stat st;
        if (stat(_httpRequest._path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                // 如果目标文件是一个目录，则默认访问该目录下的主页
                _httpRequest._path.append("/" + HOME_PAGE);
                // 获取主页的属性信息
                stat(_httpRequest._path.c_str(), &st);
            }
            else if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            {
                // 如果目标文件是一个可执行程序，则以cgi方式处理响应
                _httpRequest._cgi = true;
            }
            _httpResponse._contentLength = st.st_size; // 返回普通文件时，响应正文长度就是文件大小
            // 获取目标文件的后缀
            int pos = _httpRequest._path.rfind(".");
            if (pos == std::string::npos)
            {
                _httpResponse._suffix = ".html";
            }
            else
            {
                _httpResponse._suffix = _httpRequest._path.substr(pos);
            }
        }
        else
        {
            // 目标文件不存在
            LOG(WARNING, _httpRequest._path + " Not Found!");
            _httpResponse._statusCode = NOT_FOUND;
            goto END;
        }
        // 根据目标文件是否需要cgi进行分类处理
        if (_httpRequest._cgi == true)
        {
            _httpResponse._statusCode = ProcessCgi();
        }
        else
        {
            _httpResponse._statusCode = ProcessNonCgi();
        }
    END:
        // 构建响应报文
        BuildHttpResponseHelper();
    }
    void SendHttpResponse()
    {
        // 发送状态行
        send(_sock, _httpResponse._statusLine.c_str(), _httpResponse._statusLine.size(), 0);
        // 发送响应报头
        for (auto &line : _httpResponse._responseHeader)
        {
            send(_sock, line.c_str(), line.size(), 0);
        }
        // 发送空行
        send(_sock, _httpResponse._blank.c_str(), _httpResponse._blank.size(), 0);
        // 发送响应正文
        if (_httpRequest._cgi)
        {
            // cgi处理的结果存储在body中
            auto &body = _httpResponse._responseBody;
            ssize_t s = 0;
            ssize_t total = 0;
            const char *start = body.c_str();
            while (total < body.size() && (s = send(_sock, start + total, body.size() - total, 0)) > 0)
            {
                total += s;
            }
        }
        else
        {
            // 非cgi处理的结果就是普通的目标文件
            sendfile(_sock, _httpResponse._fd, nullptr, _httpResponse._contentLength);
        }
        /* 补充：
         * sendfile直接在内核层将文件内容拷贝至对应缓冲区
         * 而read + write是将数据拷贝至用户层，再从用户层拷贝至内核
         * 因此使用sendfile发送文件内容效率更高
         */
    }
};

// HTTP请求的入口函数
class Callback
{
public:
    void operator()(int sock)
    {
        HandleRequest(sock);
    }

private:
    void HandleRequest(int sock)
    {
        LOG(INFO, std::string("Handle request begin, socket:") + std::to_string(sock));
// #define DEBUG
#ifdef DEBUG
        // 打印请求报文
        std::cout << "-------socket:" << sock << "-------" << std::endl;
        std::string line;
        while (Util::GetLine(sock, line) > 0)
        {
            std::cout << std::to_string(sock) + ":" + line;
            line.clear();
        }
        std::cout << "end" << std::endl;
#else
        EndPoint ep(sock);
        ep.RecvHttpRequest();
        if (!ep.IsStop())
        {
            ep.BuildHttpResponse();
            ep.SendHttpResponse();
        }
        else
        {
            LOG(WARNING, "RecvHttpRequest error!");
        }
#endif
        LOG(INFO, std::string("Handle request end, socket:") + std::to_string(sock));
    }
};
