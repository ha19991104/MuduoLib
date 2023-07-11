#include "TcpConnection.h"
#include "logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"  //头文件在编译的时候，是会在源文件中展开的

#include <functional>
#include <errno.h>
#include <string>

/*
关于static：
1. static用来控制变量的存储方式和可见性，且静态变量的生命周期是从程序开始运行到结束
2. 函数内的静态局部变量存放在数据段(.data.bss),初始化在第一个函数调用的时候，且只会初始化一次(没有指定初始化的值，默认给0)，
    局部变量每次调用都会初始化；静态局部变量的作用域只在函数内，就是定义在全局数据段作用在函数内的变量。
3. 静态全局变量不能被其它文件所用；其它文件中可以定义相同名字的变量，不会发生冲突。
    和普通全局变量的区别是，可见性，全局变量其他文件是可见的，全局变量在别的文件通过extern来声明变量再进行使用
    如果想在别的文件使用静态变量，可以通过头文件包含的形式
4.静态函数 和 静态全局变量类似，不可以别其他文件外部引用，可以解决命名冲突

**Tip**: static修饰的变量都是对其他文件不可见的，就算定义在头文件中，让其他源文件包含头文件，不同源文件的对应的静态变量也是相互独立的

5.static 修饰成员变量，生存周期大于实例对象，所有对象共用一份静态成员，且不占用类的内存空间，分配到全局数据区
6.静态成员函数，也是多个实例对象共享
    静态成员函数可以访问静态成员函数和静态成员变量，不能访问非静态成员函数和非静态成员变量

**Tip**: 
    1. static修饰的成员变量只在类内声明，还需要在类外定义(不一定需要初始化，系统会默认给0)，声明是不会分配内存的，只有定义才会分配内存
    2. 类里面的普通变量int a；是即声明了又定义了，所以会分配内存。static修饰的在类里面只是声明，所以需要在类外进行定义，再分配内存
*/

static EventLoop* ChekLoopNotNull(EventLoop *loop) //静态函数，只能在当前文件可见，可以解决命名冲突的问题
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
        :loop_(ChekLoopNotNull(loop))
        ,name_(nameArg)
        ,state_(kConnecting) //正在连接
        ,reading_(true)
        ,socekt_(new Socket(sockfd))
        ,channel_(new Channel(loop,sockfd)) //最后需要注册到poller上
        ,localAddr_(localAddr)
        ,peerAddr_(peerAddr)
        ,highWaterMark_(64*1024*1024) // 64M的水位线
{
    // 给channel设置回调函数，回调函数全部用的都是TcpConnection里面的回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError,this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d/n",name_.c_str(),sockfd);
    // 启动保活机制
    socekt_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d",name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread()) //在档期那线程
        {
            sendInLoop(buf.c_str(),buf.size()); 
            //string让调用方处理，这里不需要处理
        }
        else //不是在当前线程
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop,this,buf.c_str(),buf.size()));
        }
    }
}

/*
    发送数据， 应用(非阻塞)写的快，内核发送数据慢，需要把数据写入缓冲区，而且设置了水位回调防止发送太快
*/
void TcpConnection::sendInLoop(const void* data,size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len; //还剩多少数据
    bool faultError = false; //出现错误

    //之前调用过该connetion的shutdown，不能再进行发送了
    if(state_ == kDisconnected) //再之前就已经断开连接了，已经不能发送了
    {
        LOG_ERROR("disconnected,give up writing!");
    }

    // 且缓冲区没有数据要发,就直接发data数据就可以了
    if(!channel_->isWriting() && outputBuffer_.readableBytes()==0)
    {
        nwrote = ::write(channel_->fd(),data,len);
        if(nwrote>=0) //=0不是没发出去吗？
        {
            remaining= len - nwrote; //剩余的数据
            if(remaining == 0 && writeCompleteCallback_) //数据发完了的回调
            {   
                // 到这里数据全部发送完成，就不需要给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }
        else  //nwrote<0
        {
            ssize_t nwrote = 0;
            if(errno!=EWOULDBLOCK) //没有数据发
            {   
                LOG_ERROR("TcpConnction::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET)//SIGPIPE RESET
                {
                    faultError = true; //出错了
                }
            }
        }

    }
    // 没出错，且还有数据没有发出去,需要将剩下的数据保存到缓冲区中，给poller注册可写事件(LT不需要注册)
    // epoll_wait，每次都会上报这个可写事件，直到发完了，取消可写事件
    // poller发现，tcp缓冲区有空间，就会通知相应的channel，调用handlewrite回调，一直把发送缓冲区中的数据发完
    if(!faultError && remaining>0) 
    {
        ssize_t oldLen = outputBuffer_.readableBytes(); //目前缓冲区还剩下的要发的数据
        ////要发的数据大于水位线了
        if(oldLen + remaining >= highWaterMark_ && oldLen<highWaterMark_ &&highWaterMarkCallback_) 
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));
        }
        //将没法出去的数据写到缓冲区中
        outputBuffer_.append((char*)data+nwrote,remaining);
        if(!channel_->isWriting()) //水平模式没有可写事件的话，注册一个
        {
            channel_->enableReading();
        }

    }
}
//关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setSate(kDisconnecting); //正在关闭连接
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting()) //没有写事件了,数据就已经发送完了
    {
        socekt_->shutdownWrite(); //关闭写通道，channel就会触发EPOLLHUP，调用回调closeCallback_,也就是回调handleClose
    }
}

//连接建立
void TcpConnection::connectEstablished()
{
    setSate(kConnected);// 连接建立
    channel_->tie(shared_from_this());  //监听TcpConnection对象的生命周期
    channel_->enableReading();   //挂载可读事件，向poller注册可读事件

    //新连接建立需要执行的回调
    connctionCallback_(shared_from_this()); //连接断开的时候这个也会被调用
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setSate(kDisconnected);
        channel_->disableAll(); //事件全部注销

        connctionCallback_(shared_from_this()); //销毁连接也会调用连接回调
    }
    channel_->remove(); //连接断开了，把channel从poller中移除  
    // channel是智能指针管理，TcpConnetion没有了，channel对象也就析构了，所以只需要将channel从epoll取消挂载就可以了
}



// channel上的fd有数据可读了
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno); //先收数据，数据会被写道底层的vector<char> buffer_;中
    if(n>0)
    {
        //enable_shared_from_this()获取当前对象的一个智能指针，这个是调用的外面的设置的回调
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime); //这个收到数据后是回响服务，收到什么发什么
    }
    else if(n==0) //对端对端
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if(channel_->isWriting()) //有可写事件
    {
        int savedErrno = 0;
        // 封装了一下
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes()==0)
            {
                channel_->disableWriting(); //数据发完需要将可写变成不可写
                if(writeCompleteCallback_)
                {
                    // 可以唤醒线程执行回调，TcpConnection肯定会在这个线程
                    // 这个还需看一下，如果callingPendingFunctors_不成立呢就没办法wakeup了
                    loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
                    /*
                    if(!isInLoopThread() || callingPendingFunctors_)
                    {
                        wakeup(); //唤醒loop所在线程
                    }
                    */
                }
                if(state_ == kDisconnecting) //正在关闭了
                {
                    shutdownInLoop(); //在当前所属的loop中把当前的connection关掉
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnction::handleWrite");
        }
    }
    else
    {
        // 如果已经注销了写事件，再次调用handleWrite，就会走到这里，这是一个基本的逻辑错误
        LOG_ERROR("TcpConnection fd = %d is down,no more writing",channel_->fd());
    }


}

// poller => channel::closeCallback => TcpConnection::handleClose()
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
    setSate(kDisconnected); //断开连接了
    channel_->disableAll(); //取消挂载的所有事件

    TcpConnectionPtr connPtr(shared_from_this()); //获取当前connection对象
    connctionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr); // 关闭连接的回调 ，这个回调时TcpServer给的
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err=0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s -SO_ERROR = %d",name_.c_str(),err);
}
