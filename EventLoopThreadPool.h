#pragma once
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool:noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop* baseloop,const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {numThreads_ = numThreads;} //设置线程的数量
    void start(const ThreadInitCallback& cb = ThreadInitCallback());
    
    // 如果是多线程中，baseLoop_会默认以轮旋的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops(); 

    bool started() const{return started_;}
    const std::string& name() const{return name_;}
private:
    EventLoop *baseLoop_; // 主线程的loop
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  // 包含了所有创建的线程
    // vector<EventLoop*> 里面的指针，不需要在析构的时候delete,因为EventLoppThread里面的loop_指向的栈上的EventLoop对象
    std::vector<EventLoop*> loops_;   //每个线程里面的EventLoop指针
};
