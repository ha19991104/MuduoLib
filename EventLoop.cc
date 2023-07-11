#include "EventLoop.h"
#include "logger.h"
#include "Poller.h"
#include "Channel.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
// 防止一个线程创建多个EventLoop  当一个EventLoop创建起来的时候，就指向这个对象，在一个线程里面在创建的时候，指针不为空，就不会再创建了
__thread EventLoop *t_loopInThisThread = nullptr; // t_loopInThisThread线程里面全局的EventLoop指针变量

// 定义默认的Poller IO复用接口的超时时间  10s
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}
/*


*/

EventLoop::EventLoop()
    : looping_(false)  //是否开启了循环
    , quit_(false)  // loop的退出标志
    , callingPendingFunctors_(false)  // 是否正在执行，wakeupfd的唤醒操作 
    , threadId_(CurrentThread::tid()) // 当前线程的id 
    , poller_(Poller::newDefaultPoller(this)) //返回的是 new EPollPoller(loop);
    , wakeupFd_(createEventfd()) // 创建一个唤醒fd
    , wakeupChannel_(new Channel(this, wakeupFd_)) // 给唤醒fd创建channel
    //, currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)  // 线程局部存储变量，每个线程独有一份，这个用来防止EventLoop对象被重复在一个线程里面创建
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this; //第一个创建的时候，将对象地址赋值个这个线程局部存储的指针，如果在这个线程中，再次创建，就会走进if(true)
    }
    // 设置wakeupfd的时间类型已经事件发生后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this)); // 给channl注册唤醒事件的回调，也就是读取wakeup发的8字节数据
    // 每个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading(); // 挂载到epoll上监听可读事件，当可读事件发生的时候，epoll_wait就不会阻塞
}
EventLoop::~EventLoop()  //除了下面的变量，需要手动设置外，其他的对象都是随着EventLoop的消失，出作用域调的析构
{
    wakeupChannel_->disableAll(); // 将事件置为kNoneEvent
    wakeupChannel_->remove();     // 将这个channel从Poller的挂载map上删除，然后再从epoll上取消挂载，最后将inedx设置为kNew
    ::close(wakeupFd_);       // 关闭这个句柄
    t_loopInThisThread = nullptr;  //将线程局部存储的指针设置为null，意味着这个Eventloop对象结束了
}


// 开启事件循环
void EventLoop::loop()
{
    looping_ = true; //开启已经在循环的标志
    quit_ = false;  // 通过这个来判定是否循环
    LOG_INFO("EventLoop %p start looping \n",this);
    while(!quit_)
    {
        activeChannels_.clear(); //每次监测发的事件前都需要
        // poll 监听两类fd，一种是clientfd，一种是wakeupfd
        // wakeup的时候，如果loop正在处理事情，则会在下一次循环中处理wakeup的事件
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_); //activeChannels_是传出参数，里面存放的是发生事件的channel
        for(Channel* channel:activeChannels_)
        {
            // poller能监听哪些channel发生事件了，任何上报给EventLoop,EventLoop可以通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /*
        IO线程 mianloop 主要是accept接收新用户的连接，会返回一个和客户端通信的fd，然后会用一个channel打包一个fd
        这个fd需要分发给subloop，mainloop会事先注册一个回调cb，这个cb需要subloop来执行,挂载新连接
        wakeup之后，将poll唤醒，执行wakeupChannel_读回调收取八字节数据之后doPendingFunctors()
        */
        doPendingFunctors(); //这里是mainloop注册的cb
    }
    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_ = false;
}
// 退出事件循环  1. loop在自己的线程中调用quit 2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread()) //如果不在自己的线程，则需要调用wakeup,让阻塞的循环响应，从而可以根据quit_退出loop的while循环
    {
        wakeup(); //wakeup之后，loop执行一次循环，quit_=true,不满足条件，退出循环
    }
}

//当前loop中执行callback
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())  //如果是在自己的线程就直接执行
    {
        cb();
    }
    else  //不在自己的线程
    {
        queueInLoop(std::move(cb));
    }
}
// 把cb放入队列中，唤醒loop所在的线程执行，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    //可能同时有多个其他线程会操作这个vector数组
    {
        std::unique_lock<std::mutex> lock(mutex_);
        //直接vector底层的内存上直接构造一个cb，push_back是拷贝构造
        pendingFunctors_.emplace_back(cb);  //先将cb存入其他事件的函数队列中
    }

    // 唤醒相应的需要执行上面回调操作的loop线程了
    if(!isInLoopThread() || callingPendingFunctors_) // 不在自己线程，就唤醒， 如果callingPendingFunctors_满足，就是loop正在执行这个函数，但是又有新的函数需要处理，让loop循环之后不阻塞直接处理
    {
        wakeup(); //唤醒loop所在线程
    }
}
void EventLoop::handleRead() // 用于wakeup唤醒epoll_wait的回调操作
{
    uint64_t one = 1;                               // 8个字节
    ssize_t n = read(wakeupFd_, &one, sizeof(one)); // 读也应该读八个字节
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

 // 通过向wakeupfd写数据触发他的读事件
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() reads %lu bytes instead of 8", n);
    }
}

// Eventloop的方法 => 调用poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor:functors)
    {
        functor(); //执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}
