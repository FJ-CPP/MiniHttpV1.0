#pragma once
#include <iostream>
#include <string>
#include <ctime>

/* 日志信息
 * [日志级别][时间戳][日志信息][对应文件名称][行数]                                        
 * 日志级别包括：INFO(正常日志信息)、WARNING(不影响执行的警告)、ERROR(错误)、FATAL(致命错误) 
 * 对应文件名称使用宏__FILE__                                                            
 * 行数使用宏__LINE__                                                                    
 */

enum LogLevel
{
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// '#宏参数'：将宏参数转化成字符串
#define LOG(LEVEL, MSG) Log(#LEVEL, time(nullptr), MSG, __FILE__, __LINE__)

static void Log(std::string level, time_t time, std::string msg, std::string file, int line)
{
    std::cerr << '[' << level << ']' << '[' << time << ']' << '[' << msg << ']' << '[' << file << ']' << '[' << line << ']' << std::endl;
}