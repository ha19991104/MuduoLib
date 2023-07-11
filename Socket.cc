#include "Socket.h"
#include "logger.h"
#include "InetAddress.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h> //IPPROTO_TCP
#include <unistd.h>
#include <string.h> //memset

Socket::~Socket()
{
    close(socketfd_);
}


void Socket::bindAddress(const InetAddress& localaddr)
{
    if(0!=::bind(socketfd_,(sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd : %d fail \n",socketfd_);
    }
}
void Socket::listen()
{
    if(0!=::listen(socketfd_,1024)) //加上全局作用域，以免和局部方法产生冲突 
    {
        LOG_FATAL("listen socektfd_:%d fail\n",socketfd_);
    }
}
int Socket::accept(InetAddress *peeraddr)
{
    /*
        1.accept 第三个参数的问题
        2.返回的fd需要时非阻塞的
    */
    sockaddr_in addr;
    socklen_t len = static_cast<socklen_t>(sizeof(addr)); //这个参数必须初始化
    memset(&addr,0,sizeof(addr));
    // bzero(&addr,sizeof addr); #include<strings.h>
    int connfd = ::accept4(socketfd_,(sockaddr*)&addr,&len,SOCK_NONBLOCK|SOCK_CLOEXEC); //返回非阻塞的客户端
    if(connfd>=0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(socketfd_,SHUT_WR)<0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(socketfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof(optval)));
}
void Socket::setReuseAddr(bool on)
{
     int optval = on? 1:0;
    ::setsockopt(socketfd_,SOL_SOCKET,SO_REUSEADDR,&optval,static_cast<socklen_t>(sizeof(optval)));
}
void Socket::setReusePort(bool on)
{
     int optval = on? 1:0;
    ::setsockopt(socketfd_,SOL_SOCKET,SO_REUSEPORT,&optval,static_cast<socklen_t>(sizeof(optval)));
}
void Socket::setKeepAlive(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(socketfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,static_cast<socklen_t>(sizeof(optval)));
}