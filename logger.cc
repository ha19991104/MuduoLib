#include "logger.h"
#include "Timestamp.h"
#include <iostream>
// 获取日志唯一的实例对象, 在外面就不需要static了
logger &logger::instance()
{
    static logger logger; // 这个直接就是线程安全的
    return logger;
}
// 设置日志级别
void logger::setLogLevel(int level)
{
    logLevel_ = level;
}
// 写日志的接口   [级别信息] time : xxx(msg)
void logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;

    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;

    default:
        break;
    }
    // 打印时间和msg
    std::cout << Timestamp::now().toString()
              << " : " << msg << std::endl;
}

// int main()
// {
//     LOG_INFO("%d",123);
//     return 0;
// }