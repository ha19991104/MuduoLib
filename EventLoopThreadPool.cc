#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include<memory>
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg)
    :baseLoop_(baseloop)//传入baseloop的原因是，如果没有开启任何线程的话，就需要对连接的客户端选择，getNextLoop都是返回主线程的,
    ,name_(nameArg)
    ,started_(false)
    ,numThreads_(0) 
    ,next_(0)
{}
EventLoopThreadPool::~EventLoopThreadPool()
{}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true; 

    for(int i = 0;i<numThreads_;++i)
    {
        char buf[name_.size()+32];
        snprintf(buf,sizeof(buf),"%s%d",name_.c_str(),i);
        // //底层的每个线程都会有一个name,这个name是最上层的EchoServer穿进来的,然后加上线程数量的编号
        EventLoopThread* t = new EventLoopThread(cb,buf); //创建事件loop的线程对象
        threads_.push_back(std::unique_ptr<EventLoopThread>(t)); //用于管理线程对象的
        loops_.push_back(t->startLoop()); //开启线程，并返回对应的线程的Eventloop指针,只有创建成功了还会返回
         
    }
    // 整个服务端只有一个线程，运行着baseloop
    if(numThreads_==0 && cb)
    {
        cb(baseLoop_);
    }
}

// 如果是多线程中，baseLoop_会默认以轮旋的方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_; // 如果没有子loop，每次新来的连接都是挂载到主loop上
    if(!loops_.empty())  //通过轮询获取下一个处理事件的loop
    {
        // 轮询 
        loop = loops_[next_];
        ++next_;
        if(static_cast<size_t>(next_)>=loops_.size())
        {
            next_ = 0;
        }
    }
    
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty())
    {
        return std::vector<EventLoop*>(1,baseLoop_);
    }
    else
    {
        return loops_;
    }
}
