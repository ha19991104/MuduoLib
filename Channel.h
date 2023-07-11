#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include<functional>
#include<memory>
/*
理清楚 EventLoop Channel Poller之间的关系  《= Reactor模型上对应 Demultiplex多路事件分发器
*/
class EventLoop; //这里只给声明，源文件再给具体的include,源文件最终都会编译成os库的，这样对外暴露的源文件信息会更少
class Timestamp; // 因为头文件里面也只会用到这里类的声明，并不涉及到具体的实现
// 理解为通道，封装了socketfd，和其感兴趣的event，如EPOLLIN,EPOLLOUT事件，还绑定了poller返回的具体事件
class Channel:noncopyable
{
  
public:
    //设置回调
    using EventCallback = std::function<void()>; //等价于 typedef std::function<void()> EventCallback;
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop *loop,int fd);
    ~Channel();
    
    //fd得到poller通知后，处理事件的，调用相应的回调方法处理事件
    void handleEvent(Timestamp receiveTime); //由于这里的形参receiveTime是由Timestamp定义的变量，需要确认变量的大小，因此需要包含头文件，而不只是声明
    
    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb){readCallback_ = std::move(cb);}  //cb 和 readCallback_都是左值,这里调用的function的复制操作，出了函数,局部对象cb就不需要了
    void setWriteCallback(EventCallback cb){writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb){closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb){errorCallback_ = std::move(cb);}

    // 当channel被手动remove之后，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd()const{return fd_;}
    int events()const {return events_;}
    void set_revents(int revt){revents_ = revt;} // epoll监听完之后肯定是要将发生的事件设置回来的 

    //设置fd相应的事件状态
    void enableReading(){events_ |= kReadEvent; update();} //先设置对应的事件，再通过update调用epoll_ctl
    void disableReading(){events_ &= ~kReadEvent; update();} //通过取反，来取消事件
    void enableWriting(){events_|=kWriteEvent; update();}
    void disableWriting(){events_ &= ~kWriteEvent; update();}
    void disableAll(){events_ = kNoneEvent; update();}

    // 返回当前fd的事件状态
    bool isNoneEvent()const {return events_ == kNoneEvent;} //通过这个函数可以直到这个channel的fd有没有注册感兴趣的事件
    bool isWriting()const {return events_&kWriteEvent;} //是否挂载了可写事件？
    bool isReading()const {return events_&kReadEvent;}

    int index(){return index_;}
    void set_index(int idx){index_ = idx;}

    //one thread one loop 当前channel属于哪个EventLoop
    EventLoop* ownerLoop() {return loop_;}
    void remove(); 
    
private:
    void update(); // 更新
    void handleEventWithGuard(Timestamp receivce);

    // 三个static表示fd的状态
    static const int kNoneEvent; // 没有对任何事件感兴趣
    static const int kReadEvent; // 对读事件感兴趣
    static const int kWriteEvent;// 对写事件感兴趣

    EventLoop* loop_; // 事件循环
    const int fd_;  // fd，Poller监听的对象
    int events_;    // 注册fd的事件
    int revents_;   // Poller返回的具体发生的事件
    int index_;     //  初始化为-1，表示当前channel状态是还没有被挂载到epollfd上的

    std::weak_ptr<void> tie_; // 防止channel被手动remove之后还在使用，跨线程监听
    bool tied_;

    //引用channel通道里面，能够获知fd最后具体发生的事件revents_，所以channel还需要负责调用具体事件的回调操作
    ReadEventCallback readCallback_;  //这些回调肯定是用户指定的
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
