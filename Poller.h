#pragma once
#include "noncopyable.h"
#include "Timestamp.h"
#include<vector>
#include<unordered_map>
class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用函数
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop* loop);
    virtual ~Poller()=default;

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels)=0; //基类保留统一的接口，实现不同的IO复用机制
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数channel是否在当前Poller的map容器当中
    bool hasChannel(Channel* channel)const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // 这里map 的 key 表示 socketfd  value： socketfd所属的channel
    // 通过socketfd查找相应的channel
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerloop_; //定义Poller所属的事件循环EventLoop
};