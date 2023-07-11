#pragma once
#include "noncopyable.h"
#include <functional>
#include <thread>
#include <memory>
#include <string>
#include <atomic>
#include <unistd.h>
#include <thread>
/*
    线程肯定要有一个线程函数
*/
class Thread:noncopyable
{ 
public:
    using ThreadFunc = std::function<void()>;  //如果要带参数，就可以使用绑定器
    explicit Thread(ThreadFunc,const std::string &name = std::string());
    ~Thread();

    void start(); 
    void join();

    bool started() const {return started_;}
    pid_t tid() const {return tid_;} // 不是pthrad_self打印出来的pid
    const std::string& name()const{return name_;}

    static int numCreated() {return numCreated_;}
private:
    void setDefaultName();

    bool started_;
    bool joined_;
    //std::thread thread_; // 因为这个定义了线程就启动了，所以考虑需要通过智能指针来封装  控制产生的时间
    std::shared_ptr<std::thread> thread_; // 
    pid_t tid_;
    ThreadFunc func_; //存储线程函数的
    std::string name_; 
    static std::atomic_int numCreated_; // 用于表示线程数量  静态成员变量要在内外单独的进行初始化
};
