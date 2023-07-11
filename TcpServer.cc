#include "TcpServer.h"
#include "logger.h"
#include "TcpConnection.h"
#include <strings.h>

static EventLoop* ChekLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null! \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}
 
/*

*/
TcpServer::TcpServer(EventLoop* loop, const InetAddress &listenAddr,const std::string& nameArg,Option option)
    :loop_(ChekLoopNotNull(loop)) //主线程loop
    ,ipPort_(listenAddr.toIpPort())
    ,name_(nameArg)
    ,acceptor_(new Acceptor(loop,listenAddr,option == kReusePort)) // Acceptor底层创建了侦听fd
     //构造函数里面封装了侦听socket的channel，
    ,threadPool_(new EventLoopThreadPool(loop,name_)) //创建了事件循环的线程池，为什么要传入主线程的loop呢,因为没有连接的话就需要每次都通过getNextLoop返回主线程的loop去挂载客户端的fd
    ,connctionCallback_() //
    ,messageCallback_()
    ,nextConnId_(1)
    ,started_(0)
{
    // 当有新用户连接时，触发可读事件，然后在channel的可读事件里面调用回调newConnection
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                                  this,
                                                  std::placeholders::_1,std::placeholders::_2));
}

TcpServer::~TcpServer() //主线程的loop是在main函数结束后才释放的，而且整个线程的循环都是在loop中，如果有一些其他的时候要做就需要通过runInLoop的方式
{
    for(auto &item:connctions_)
    {
        TcpConnectionPtr conn(item.second); //让局部的智能指针对象指向item.second，这个局部指针，出作用域自动释放new出来TcpConnetion对象
        item.second.reset(); //让TcpConnection的强智能指针不在指向原来的对象了

        //销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed,conn)); //让每个conntion的对应的loop去销毁连接，就是注销channel的挂载
    }
}


//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads); // 
}

// 开启服务器服务器监听，底层开启的是Acceptor listen
void TcpServer::start()
{
    if(started_++ == 0)  //防止一个TcpServer对象被start多次
    {
        //threadInitCallback_这个回调在哪里设置？  没有被设置
        threadPool_->start(threadInitCallback_); //现在外面设置线程数量，最后再启动线程
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get())); 
    }
}

//有一个新的客户端的连接, acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr)
{
    EventLoop *ioloop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf,sizeof(buf),"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_; //对这个变量处理只在主线程，所以不会有线程安全问题
    std::string connName = name_+buf; // 最外层用户传进来的name+新连接的buf，组测

    LOG_INFO("TcpServer::newConnetion [%s] -new connection [%s] from %s \n",
             name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());
    // 通过sockfd获取器绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof(local));
    socklen_t addlren = sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addlren)<0)
    {   
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 给clientfd创建一个TcpConnection对象，会在这个对象创建的时候，并给fd创建相应的channel(绑定事件和读写回调)和socket()
    TcpConnectionPtr conn(new TcpConnection(ioloop,
                                            connName,
                                            sockfd,
                                            localAddr, //本端的ip地址和端口
                                            peerAddr));//对端的ip地址和端口
    connctions_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer=>TcpConnetion=>channel=>Poller=>channel调用回调
    conn->setConnectionCallback(connctionCallback_); //连接成功的回调(就channel已经挂载到epoll上了)
    conn->setMessageCallback(messageCallback_);      // 收到消息后的处理回调，在这里的话就是回响服务
    conn->setWriteCompleteCallback(writeCompleteCallback_); //数据发完的了回调

    //设置如何关闭连接的回调  用户会调用conn的shutdown
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));

    //直接调用 TcpConnection::connectEstablished
    ioloop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));

}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] -connection %s\n",name_.c_str(),conn->name().c_str());

    ssize_t n = connctions_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();

    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
}
