#pragma once
#include <string>

#include "noncopyable.h"

// 日志级别 INFO(打印重要的流程信息)
// ERROR(错误，不影响正常执行)
// FATAL(系统无法正常运行下去的错误日志)
// DEBUG(调试信息)

   
// “\ 起到的换行 \后面不能+空格”
/* 需要补充，搞清楚
#  :    #8 => "8"  这个8就是个字符串
## :    L##x => L"x"  //##是c的语法，将两个字符串连接在一起
__VA_ARGS__: 需要配合define使用，#define myprintf(...) printf( __VA_ARGS__) 将...的内容抄到__VA_ARGS__位置上
##__VA_ARGS__：当可变参个数为0的时候，##可以将前面多余的","去掉,不然会报错


*/
/*
    int snprintf(char *str, size_t size, const char *format, ...);
    参数一：目标字符串，用于存储格式化后的字符串数组的指针
    参数二：字符数组的大小(str最多能接收的字符数),输入到str的字符数超过size会被截断，最多只能写入size-1,最后一个是'\0',如果没有超过size,就全部复制到str中，末位补'\0'
    参数三：格式化字符串
    ...可变参数，会根据参数三种的格式化指令进行格式化
    返回值，是想要写进str的长度，这个长度不包含'\0'
*/ 
//// LOG_INFO("%s %d",arg1,arg2)
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        logger &logger = logger::instance();              \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0) // 防止宏展开有意想不到的错误

#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        logger &logger = logger::instance();              \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0) // 防止宏展开有意想不到的错误

#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        logger &logger = logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1);                              \
    } while (0) // 防止宏展开有意想不到的错误
// 对于调试信息默认是关闭的，东西太多了
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        logger &logger = logger::instance();              \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0) // 防止宏展开有意想不到的错误
#else
    #define LOG_DEBUG(logmsgFormat, ...)   
#endif
enum LogLeve1
{
    INFO,  // 正常信息
    ERROR, // 错误信息
    FATAL, // core信息，导致程序直接错误的
    DEBUG, // 调试信息
};
// 输出一个日志类,单例模式
class logger : noncopyable // 不允许进行拷贝构造和赋值
{
public:
    // 获取日志唯一的实例对象
    static logger &instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志的接口
    void log(std::string msg);

private:
    int logLevel_; // muduo里面都是给成员变量后面+_
    logger() {}    // 构造函数私有化
};
