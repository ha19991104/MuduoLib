#include "Poller.h"
#include "Channel.h"
Poller::Poller(EventLoop* loop)
    :ownerloop_(loop){}


bool Poller::hasChannel(Channel* channel)const
{
    auto it = channels_.find(channel->fd());
    return it!=channels_.end()&&it->second  == channel;
}


// Poller* Poller::newDefaultPoller(EventLoop* loop) //这个函数里面会生成一个具体的IO复用对象，并返回一个基类的指针
// {}  // 所以在类外实现，肯定会多包含两个头文件#include "PollPoller.h"#include "EpollPoller.h"
// Poller是基类，派生类可以引用基类，基类不能引用派生类