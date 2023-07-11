#pragma once

/*
* 用户使用muduo编写服务器程序
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Timestamp.h"

#include <functional>
#include <memory>
#include <atomic>
#include <unordered_map> //底层是哈希表,考虑到string不需要排序
#include <string>

// 对外的服务器编程使用的类
class TcpServer
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; //在EventLoopThread::threadFunc()里面的callback运行
    enum Option
    {
        kNoReusePort,
        kReusePort
    };
    TcpServer(EventLoop* loop,const InetAddress &listenAddr,const std::string& nameArg,Option option = kNoReusePort);
    ~TcpServer();

    const std::string& ipPort() const {return ipPort_;}
    const std::string& name() const {return name_;}
    EventLoop* getLoop()const {return loop_;}

    void setThreadInitCallback(const ThreadInitCallback& cb){threadInitCallback_ = cb;} //用于线程创建，创建成功的回调？并没有设置过
    void setConnectionCallback(const ConnctionCallback& cb) {connctionCallback_ = cb;} // 
    void setMessageCallback(const MessageCallback &cb) {messageCallback_ = cb;}  //
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_ = cb;} //

    //设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器服务器监听，底层开启的是Acceptor listen
    void start();

    // 
private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string,TcpConnectionPtr>;

    EventLoop *loop_;  //主线程loop
    const std::string ipPort_;  //服务器的ip、port
    const std::string name_;     //

    std::unique_ptr<Acceptor> acceptor_; //运行在主线程loop，任务是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_; //

    ConnctionCallback  connctionCallback_;  //有新连接的回调
    MessageCallback messageCallback_;       //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息写完之后的回调
    
    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connctions_;  //保存所有的连接
};
