#include "EventLoopThread.h"
#include "EventLoop.h"
//#include <memory>
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb
            ,const std::string& name)
            : loop_(nullptr)
            , exiting_(false)
            , thread_(std::bind(&EventLoopThread::threadFunc,this),name) //底层的线程绑定了线程函数，需要再EventLoopThread::startLoop()中启动
            ,mutex_()  //为什么要默认构造
            ,cond_()
            ,callback_(cb)
{}
EventLoopThread::~EventLoopThread()
{ 
    exiting_ = true;
    if(loop_!=nullptr)
    {
        loop_->quit();  // 先让循环的quit_标志变成true，然后wakeup让loop在下次循环的时候直接停止
        thread_.join(); //等待线程结束，回收资源
    }

}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); //启动底层线程
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock); //只有等到底层的Eventloop创建好了，才会被唤醒
        }
        loop = loop_;
    }
    return loop;  //返回临时变量？？？？ 脑残？
}

// 下面这个方法，是在单独的新线程中运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop;  //创建一个独立的eventloop,和上面的线程是一一对应的，one loop per thread
    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; // 通过指针指向 句柄对象
        cond_.notify_one();
    }
    loop.loop();  //这里就开启是子线程循环了 
    std::unique_lock<std::mutex> lock(mutex_); //有加锁的必要吗？
    loop_ = nullptr; // 
}