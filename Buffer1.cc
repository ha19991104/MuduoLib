#include "Buffer1.h"
#include <errno.h>
#include <sys/uio.h> //iovec
#include <unistd.h>

/*
    从fd上读取数据，底层poller工作在LT模式
    Buffer缓冲区是有大小的，但是从fd上读取数据的时候，却不知道tcp数据最终的大小
*/
/*
ssize_t readv(int fd, const struct iovec *iov, int iovcnt); --可以一次指定多个缓冲区进行写入，可以减少系统调用？
可以同时把数据读到多个iovec元素里面，先填满前面的容器，前面的容器不够，就往后面填充，可以用于，
比如收到了两个包，一个包是10个字节一个包是20字节，不想判断包的范围，可以传两个iovec对象， 
一个iov_base是10字节，一个iov_base是20字节，这样就收满了，同样可以对文件的句柄进行操作
//v是向量的意思
//const struct iovec *iov 可以传多个结构体数组，由iovec组成的结构体数组
//iovcnt 结构体数量
struct iovec {
    void  *iov_base;     //可以自定义一个结构
    size_t iov_len;      // 自定义结构的长度
};
*/

ssize_t Buffer::readFd(int fd,int* saveErrno)
{
    char extrabuf[65536]={0};   //栈上的内存空间，内存分配效率高 ，函数结束，内存就被回收了
    // 65536 =64k?
    struct iovec vec[2];

    const size_t writable = writableBytes(); //底层缓冲区Buffer剩余的可写空间大小
    vec[0].iov_base = begin()+writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);
    //先填充vec[0],vec[0]填充满了，再填充vec[1], 如果vec[0]够了，就不会需要多余的vec[1]
    // 如果extrabuf里面有内存，就会添加到Buffer里面，这样最后的结果就是缓冲区是刚刚好存放所有要写入的内容
    // 没有空间浪费，Buffer会根据extrabuf里面的内容进行对应的扩容，内存空间利用率高

    const int iovcnt = (writable<sizeof(extrabuf))?2:1;  //一次最多读64k数据？
    const ssize_t n = ::readv(fd,vec,iovcnt);  
    if(n<=0)
    {
        *saveErrno = errno;
    }
    else if(n<=writable)
    {
        writerIndex_+=n;//数据通过readv直接放进缓冲区中，因为传入的是缓冲区的地址
    }
    else  //n>writable，数据写入extrabuf
    {
        writerIndex_ = buffer_.size();
        append(extrabuf,n-writable); // 从buffer.size();开始写入n-writable字节
    }
    return n;
}

 ssize_t Buffer::writeFd(int fd,int* saveErrno)
 {
    ssize_t n = ::write(fd,peek(),readableBytes()); //从peek位置开始，写readableBytes的数据
    if(n<=0)
    {
        *saveErrno = errno;
    }
    return n;
 }