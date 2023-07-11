#pragma once

#include <vector>
#include <string>
#include <algorithm>
//参考的Java的第三方的netty
//网络库底层的缓冲区类型定义

	/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
	///
	/// 
	/// +-------------------+------------------+------------------+
	/// | prependable bytes |  readable bytes  |  writable bytes  |
	/// |                   |     (CONTENT)    |                  |
	/// +-------------------+------------------+------------------+
	/// |                   |                  |                  |
	/// 0      <=      readerIndex   <=   writerIndex    <=     size
	/// 
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;  //用于记录数据包的长度
    static const size_t kInitialSize = 1024; //缓冲区大小1024

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend+initialSize) //给vector底层默认开辟这么大长度
        ,readerIndex_(kCheapPrepend)
        ,writerIndex_(kCheapPrepend)
    {}
    ~Buffer()=default;
    
    //可读字节
    size_t readableBytes()const {return writerIndex_-readerIndex_;}
    // 可写字节
    size_t writableBytes()const{return buffer_.size()-writerIndex_;}

    size_t prependableBytes() const {return readerIndex_;}
    
    //返回缓冲区中，可读数据的起始地址
    const char* peek()const {return begin()+readerIndex_;}

    // 
    void retrieve(size_t len)
    {
        if(len<readableBytes()) // 只读取了len长度，还有部分没有读完
        {
            readerIndex_ += len; //应用只读取了缓冲区数据的一部分，就是len长度，还剩下readerIndex_ += len; -> writerIndex_没读
        }
        else  // len == readableBytes();
        {
            retrieveAll();
        }
    }

    void retrieveAll()  //全部复位
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); //应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(),len);  //peek()可读数据的起始地址，len可读数据的长度
        retrieve(len);  // 上一句将缓冲区中可读数据读出来了，这里需要将缓冲区进行复位操作
        return result;
    }

    // 可写的缓冲区是  buffer_.size - writerIndex_;
    void ensureWritableBytes(size_t len)  // len是要写入的长度
    {
        if(writableBytes()<len) //需要扩容
        {
            makeSpace(len); // 需要扩容
        }
    }

    // 把[data,data+len]内存上的数据添加到writable缓冲区中
    void append(const char *data,size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data,data+len,beginWrite());
        writerIndex_ += len;
    }
    
    // 开始可以写的地方
    char* beginWrite()
    {
        return begin()+writerIndex_;
    }
    // 用于常对象调用
    const char* beginWrite() const
    {
        return begin()+writerIndex_;
    }

    //从fd上读取数据
    ssize_t readFd(int fd,int* saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);

private:
    char* begin() {return &*buffer_.begin();}  // 先是调用迭代器的it.operator*(),访问底层首元素元素，再对底层首元素取地址
    const char* begin()const {return &*buffer_.begin();} //常方法，给常对象用的
    
    void makeSpace(size_t len)
    {
        if(writableBytes()+prependableBytes()<len + kCheapPrepend) //可写的+前面被读之后空闲的 全部空间依然不够，就需要扩容
        {
            buffer_.resize(writerIndex_+len);
        }
        else // 可写的+前面被读之后空闲的 全部空间  可以写下新内容 
        {
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_,
                    begin()+writerIndex_,
                    begin()+kCheapPrepend);  // 将begin()+readerIndex_ 和 begin()+writerIndex_之间的数据拷贝到kCheapPrepend开始的位置
                  readerIndex_ =   kCheapPrepend;
                  writerIndex_ = readerIndex_+readable;
        }
    }
    
    std::vector<char> buffer_; //底层内存直接通过vector来管理的,所以Buffer不需要字节析构了，当前对象析构的时候，成员对象析构
    size_t readerIndex_;
    size_t writerIndex_;
};
