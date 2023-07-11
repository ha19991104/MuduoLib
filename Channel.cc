#include "Channel.h"
#include "EventLoop.h"
#include "logger.h"
#include <sys/epoll.h>
// 不管是静态的成员变量还是成员方法都不需要添加static
// 类内声明，类外实现，不然不会分配内存
const int Channel::kNoneEvent = 0;                  // 没有对任何事件感兴趣
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 对读事件感兴趣
const int Channel::kWriteEvent = EPOLLOUT;          // 对写事件感兴趣



/*
channel 绑定fd和对应的注册事件，还有发生的事件，每当channel被创建的时候，都需要注册对应的事件的回调函数

*/


Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() //没有什么需要的外部资源，整个对象出对应的作用域就析构了 
{}

// 在什么时候调用这个方法  //void TcpConnection::connectEstablished() 新连接创建的时候
void Channel::tie(const std::shared_ptr<void> &obj)  //channel_->tie(shared_from_this());
{
    // 触发事件调用的是connection的回调，再调用过程中，需要保证，对应的connection对象还活着
    // 所以通过shared_from_this返回一个指向TcpConnection的shared指针，传给channel
    tie_ = obj; //std::weak_ptr<void> tie_;  通过弱智能指针，监听对象是不是还活着
    // 只有后面tie.lock提升成功，才会调用相应的回调
    tied_ = true;
}

// 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件,epoll_ctl
// channel本身想向Poller里面注册事件是做不到的，因为没有poller对象，所以肯定是通过EventLoop来做这件事
// EventLoop => ChanneList Poller
void Channel::update()
{
    // 通过channel所属的eventlool,调用poller的相应方法，注册fd的events事件
     loop_->updateChannel(this);   //进而调用的poller的updateChannel
}

// 在channel所属的Eventloop中，将当前的channel删除
void Channel::remove()
{
     loop_->removeChannel(this);
}

// fd得到Poller通知之后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if(tied_) //是否绑定过，就是std::weak_ptr<void> tie_;是否监听当前channel
    {
        guard = tie_.lock(); //将弱智能指针提升为强智能指针
        if(guard) //guard不为nullptr，表示当前channel存活
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发送的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receivce)
{
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    // 发生异常了
    if((revents_&EPOLLHUP)&&!(revents_&EPOLLIN)) //EPOLLHUP：TCP连接被对端关闭，或者关闭了写操作
    { // EPOLLHUP这个事件是不用向专门向epoll注册的，如果是shutdown半关闭，最后还是会触发这个事件
        if(closeCallback_)
        {
            closeCallback_(); //这里最后会调用TcpConnection的handleclose方法
        }
    }
    if(revents_&(EPOLLERR))
    {
        if(errorCallback_)
            errorCallback_();
    }
    if(revents_&(EPOLLIN|EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receivce);
        }
    }
    if(revents_&EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}