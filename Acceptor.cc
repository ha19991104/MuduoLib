#include "Acceptor.h"
#include "logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sokcfd = ::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0); 
    //第三个可以写成0，因为第二个参数是的SOCK_STREAM的话，默认就会使IPPROTO_TCO;
    if(sokcfd<0)
    {
        LOG_FATAL("%s:%s:%d linsten socket create err:%d\n",__FILE__,__FUNCTION__,__LINE__,errno);
        //__FILE__ 打印当前文件的文件名
        //__FUNCTION__ 打印函数名
        //__LINE__ 行数
    }
    return sokcfd;
}



Acceptor::Acceptor(EventLoop *loop,const InetAddress &listenAddr, bool reuseport)
    :loop_(loop) // 有了loop才能把当前的channel丢给poll
    ,acceptSocket_(createNonblocking())  //创建套接字
    ,acceptChannel_(loop_,acceptSocket_.fd()) //channel 依赖loop是因为，需要向poll上不断注册或者修改，channel无法直接访问poll的
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); //绑定套接字
    // TcpServer::start() Accept.listen  运行起来，有新用户的连接，需要执行一个回调(需要将connfd打包成channel，再去唤醒subloop)
    // baseLoop监听到acceptChannel_有事件发生的话  就会调用读事件的回调
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));  
}


Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true; //开始监听了
    acceptSocket_.listen();
    acceptChannel_.enableReading(); // listen之后需要在poll上注册可读事件
}

void Acceptor::handleRead()  //侦听fd收到新连接的时候执行
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd>=0)
    {
        if(newConnectionCallback_) //这个具体的回调是由TCPServer给到的
        {
            newConnectionCallback_(connfd,peerAddr); //轮询找到subloop, 唤醒，分发当前的新客户端的channel
        }
        else
        {
            ::close(connfd); //如果没有回调去挂载，就直接close
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d\n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno==EMFILE) //进程句柄资源数达到上限了，不同的服务器，处理方式不同
        {
            LOG_ERROR("%s:%s:%d fd reached limit! \n",__FILE__,__FUNCTION__,__LINE__);
        }
    }
}