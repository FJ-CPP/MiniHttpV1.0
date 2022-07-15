#pragma once
#include <iostream>
#include <string>
#include <unistd.h>

void CutString(const std::string &str, const std::string &sep, std::string &sub1, std::string &sub2)
{
    int pos = str.find(sep);
    sub1 = str.substr(0, pos);
    sub2 = str.substr(pos + sep.size());
}

void GetQuery(const std::string &method, std::string &query)
{
    if (method == "GET")
    {  
        if (getenv("QUERY") == nullptr)
        {
            std::cerr << "getenv nullptr" << std::endl;
        }
        query = getenv("QUERY");
    }
    else if (method == "POST")
    {
        int contentLength = atoi(getenv("CONTENT_LENGTH"));
        char ch = 0;
        for (int i = 0; i < contentLength; ++i)
        {
            read(0, &ch, 1);
            query.push_back(ch);
        }
    }
}
