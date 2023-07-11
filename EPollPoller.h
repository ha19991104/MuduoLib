#pragma once

#include "Poller.h"  //需要包含Poller的头文件，不仅仅需要前置声明
#include "Timestamp.h"
#include "Channel.h"

#include<vector>
#include<sys/epoll.h>

//class Channel;
/*
    epoll的使用
    epoll_create  构造析构
    cpoll_ctl  add/mod/del  updateChannel  removeChannel
    epoll_wait  poll里面
*/
class EPollPoller:public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override; //覆盖，基类里面这个方法一定是虚函数
    
    //重写基类Poller的抽象方法 
    Timestamp poll(int timeoustMs,ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    static const int kInitEventListSize = 16; //vector<epoll_event> 初始的长度   //只有static const int 可以在类里面初始化

    //填写活跃的连接 
    void fillActiveChannels(int numEvents, ChannelList *activeChannels)const;
    // 更新channel通道
    void update(int operation, Channel* channel);  //调用的epoll_ctl

    using EventList = std::vector<epoll_event>;
    int epollfd_;
    EventList events_; //用于保存传出参数？
};