#pragma once 

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer1.h"
#include "Timestamp.h"
#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;
// 成功打包，客户端连接服务器的一条链路
/*
    TcpServer => Acceptor =>有一个新用户连接，通过accept函数拿到connfd => TcpConnetion 设置回调 => channel => poller =>事件发送就会调用Channel的回调操作
*/
class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                  const std::string& name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop()const {return loop_;} //TcpConnection的事件循环
    const std::string& name()const {return name_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}

    bool connected() const {return state_ == kConnected;}
    
    //发送数据
    //void send(const void *message,int len);
    void send(const std::string &buf);
    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnctionCallback& cb)
    {
        connctionCallback_ = cb;
        
    }

    void setMessageCallback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }

    //设置成功发完数据执行的回调
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    //连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE{kDisconnected,kConnecting,kConnected,kDisconnecting};

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message,size_t len);

    void shutdownInLoop();

    void setSate(StateE state){state_ = state;}

    EventLoop* loop_; //这里绝对不是baseloop,TcpConnetion都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;  //表示连接的状态
    bool reading_;

    //这个和Acceptor类似，Acceptor在主线程，tcpconnection在子线程
    std::unique_ptr<Socket> socekt_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_; //当前主机的
    const InetAddress peerAddr_; //对端的

    // 函数回调是在通过TcpServer设置，再在TcpConnection中传给channel，poller再监听channel
    ConnctionCallback  connctionCallback_;  //有新连接的回调
    MessageCallback messageCallback_;       //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息写完之后的回调
    HighWaterMarkCallback highWaterMarkCallback_; // 超高水位的回调
    CloseCallback closeCallback_; 

    size_t highWaterMark_; //水位标志

    Buffer inputBuffer_;  //接收的
    Buffer outputBuffer_; //发送的
};

