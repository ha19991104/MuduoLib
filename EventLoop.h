#pragma once
#include<functional>
#include<vector>
#include<atomic> //原子操作
#include<memory>
#include<mutex>
#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
class Channel;
class Poller;

// 事件循环类  主要包含了两个大模块  Channel   Poller(epoll抽象)
// 一个事件循环包含一个Poller，一个Poller肯定有多个Channel
class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop(/* args */);
    ~EventLoop();
    
    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime()const {return pollReturnTime_;}

    //当前loop中执行
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程执行，执行cb 
    void queueInLoop(Functor cb);

    // 用来唤醒loop所在的线程
    void wakeup();

    // Eventloop的方法 => 调用poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断Eventloop对象是否在自己的线程里面
    bool isInLoopThread()const{return threadId_==CurrentThread::tid();} //判断这个eventloop对象是不是在自己的线程中


private:
    void handleRead(); //用于wakeup
    void doPendingFunctors(); //执行回调，回调都在vector容器里面放着

    using ChannelList = std::vector<Channel*>;  //用于管理channel

    std::atomic_bool looping_; // 原子操作，底层是通过CAS实现的
    std::atomic_bool quit_; // 标志退出loop循环
 
    const pid_t threadId_;  //记录当前所在线程的id
    
    Timestamp pollReturnTime_; //记录的是Poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // 主要作用，当mianLoop获取一个新用户的channel，通过轮询算法选择一个subloop,通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel>  wakeupChannel_; //用于封装wakeupFd_，poller里面都是操作的channel不会直接操作fd

    ChannelList activeChannels_; 
    // Channel *currentActiveChannel_; 用于遍历activeChannels_做断言用的，可以用不到

    std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    //存储loop需要执行的所有的回调操作
    std::mutex mutex_;  // 互斥锁，用来保护vector容器的线程安全操作

};
