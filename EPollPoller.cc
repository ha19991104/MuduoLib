#include "EPollPoller.h"
#include "logger.h"
//#include "Channel.h"

#include<errno.h>  //errno
#include <unistd.h> //close
#include<string.h>  //memset
const int kNew = -1; //channel还没有添加到epollfd上  ， channel第一次添加多的时候，channel成员index_为-1
const int kAdded = 1; //一个channel已经添加到epollfd上了
const int kDeleted = 2;  //channel从poller中删除
// 如果index_是1 再调用update，表示就是要更改你感兴趣的事件

EPollPoller::EPollPoller(EventLoop* loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC)) //EPOLL_CLOEXEC epoll的标志位
    ,events_(kInitEventListSize)    // 对事件结构体数组，进行初始化，默认长度kInitEventListSize=16
{
    if(epollfd_<0)
    {
        LOG_FATAL("epoll_create error:%d \n",errno);  //通过日志级别来判断程序是否能继续执行
    }

}

EPollPoller:: ~EPollPoller()
{
    ::close(epollfd_);
}

// 通过EventLoop调用, 将EventLoop中的ChannelList传进来，poll就是通过epoll_wait监听到哪些fd发生了事件
// activeChannels是个传出参数？将发生事件的channel告知给EventLoop
Timestamp EPollPoller::poll(int timeoustMs,ChannelList* activeChannels)
{
    //  实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu\n",__FUNCTION__,channels_.size()); //如果不是debug不要调用，因为高并发会影响poll的性能
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),static_cast<int>(events_.size()),timeoustMs);
    //using EventList = std::vector<epoll_event>;EventList events_;   events_是个vector，底层是个数组
    // events_.begin()返回首元素的迭代器,需要解引用*events_.begin()，这个是首元素在取值
    // events_.size()返回的是size_t，而epoll_wait需要的是int   events_是传出参数
    int savedErrno = errno; // 先获取errno，防止多线程修改全局变量errno
    Timestamp now(Timestamp::now());

    if(numEvents>0) //有事件发生
    {
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size()) // epoll_wait默认是水平模式，如果numEvents和长度一样,可能会有事件没有处理，所以需要扩容
        {
            events_.resize(events_.size()*2);
        }
    }
    else if (numEvents == 0) //暂时没有事件，等待到超时了
    {
        LOG_DEBUG("%s timeout! \n",__FUNCTION__);
    }
    else
    {
        if(savedErrno!=EINTR) //如果不是被中断了,就需要报错
        {
            errno = savedErrno;  //为什么要重新赋值回来，因为原来muduo中，日志获取的是全局errno的错误
            LOG_ERROR("EPollPoller::poll() err!");  //为什么用ERROR记录而不是FATAL
        }
    }
    return now;
}

// channel的update和remove 通过调用 EventLoop的updateChannel和removeChannel，最后调用到Poller相应的函数
/*
             EventLoop
    ChannelList         Poller
存放所有的channel    ChannelMap<int,channel*> channels_，如果一个channel注册过，就放入ChannelMap中
*/
void EPollPoller::updateChannel(Channel* channel) // 根据channel的index来进行相应的epoll_ctl操作
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d envents=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index == kNew || index == kDeleted) //为什么会有kDeleted这种情况
    {
        if(index == kNew) //第一次添加就直接添加，如果是已经删除的就继续添加上去
        {
            int fd = channel->fd();
            channels_[fd] = channel; //先放入到挂载的map容器里面，后面就说明这个fd一直在epoll上挂载
        }
        channel->set_index(kAdded); // 标志设置为已经挂载
        update(EPOLL_CTL_ADD,channel); //挂载操作
    }
    else  //channel本来就在epoll上 要做的操作是 EPOLL_CTL_MOD/DEL
    {
        int fd = channel->fd(); //先获取fd
        if(channel->isNoneEvent()) // 如果没有事件的话，就是DEL操作
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);//将index设置为已删除，那么这个channel还会存在吗，不存在的话，什么时候结束，存在的话，什么时候挂载上来
        }
        else //可能channel里面的事件变了
        {
            update(EPOLL_CTL_MOD,channel);  //事件有变进行修改
        }
    }
}

void EPollPoller::removeChannel(Channel* channel)  //需要线程挂载的map中删除，再从epoll删除，最后设置index为new，表示没被挂载
{
    int fd = channel->fd();
    channels_.erase(fd);  //从Poller的挂载集合里面删除

    LOG_INFO("func=%s => fd=%d \n",__FUNCTION__,fd);

    int index = channel->index();
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew); //重新设置，表示没有挂载过,只存在于ChannelList
}




//填写活跃的连接 
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels)const
{
    for(int i = 0;i<numEvents;++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        //int fd = channel->fd();
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); //EventLoop就拿到了poller给他返回的所有发生事件的channel

    }
}
// 更新channel通道  epoll_ctl的 add/mod/del
void EPollPoller::update(int operation, Channel* channel)  //调用的epoll_ctl
{
    struct epoll_event event;
    memset(&event,0,sizeof(event));
    //bzero(&event,sizeof(event));

    int fd = channel->fd();

    event.events = channel->events(); //填写的是感兴趣的事件
    //event.data.fd = fd;
    event.data.ptr = channel;
    

    if(::epoll_ctl(epollfd_,operation,fd,&event)<0) //成功是0，失败返回-1
    {   // 修改失败
        if(operation==EPOLL_CTL_DEL)  //如果只是delete挂载的话，还是可以容忍的
        {
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else  //如果是修改或者是新增，就不能向下执行了
        {
            LOG_FATAL("epoll_ctl del error:%d\n",errno);  // LOG_FATAL 会自动退出
        }

    }


}
