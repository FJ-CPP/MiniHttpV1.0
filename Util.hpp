#pragma once
#include <string>
#include <sys/types.h>
#include <sys/socket.h>

// 功能函数类
class Util
{
public:
    // 将报文按照行进行读取，其中不同系统的行末字符不同，可能为'\n'、'\r'或'\r\n'
    // 这里将它们统一处理为'\n'
    static int GetLine(int sock, std::string &line)
    {
        char ch = 'c';
        while (ch != '\n')
        {
            ssize_t s = recv(sock, &ch, 1, 0);
            if (s > 0)
            {
                // 正常读取到一个字符
                if (ch == '\r')
                {
                    if (recv(sock, &ch, 1, MSG_PEEK) > 0) // MSG_PEEK表示仅查看数据，但不将其从缓冲区读走
                    {
                        if (ch != '\n')
                        {
                            ch = '\n';
                        }
                        else
                        {
                            // 下一个字符为\n说明系统以'\r\n'为换行，此时将下一个字符读走
                            recv(sock, &ch, 1, 0);
                        }
                    }
                }
                line.push_back(ch);
            }
            else if (s == 0)
            {
                // 对端关闭连接
                return 0;
            }
            else
            {
                // 读取出错
                return -1;
            }
        }
        return line.size();
    }
    static void CutString(const std::string &str, const std::string &sep, std::string &sub1, std::string &sub2)
    {
        int pos = str.find(sep);
        sub1 = str.substr(0, pos);
        sub2 = str.substr(pos + sep.size());
    }
};