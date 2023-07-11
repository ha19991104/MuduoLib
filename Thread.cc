#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>
std::atomic_int Thread::numCreated_(0); // 如果是用等号构造的话，对应的构造函数，用了explicit就用不了

Thread::Thread(ThreadFunc func, const std::string &name)
    :started_(false)
    ,joined_(false)
    ,tid_(0) //线程函数开启的时候会自动给一个系统下的线程id
    ,func_(std::move(func))
    ,name_(name)
{
    setDefaultName();
}
Thread::~Thread()
{   
    // started_ 线程已经运行了,就需要回收
    /*
    1. linux线程有两种状态，joinable和unjoinable状态 
        joinable：线程自己返回或pthread_exit都不会自己释放线程所占用的资源(堆栈、线程描述符)，只有调用ptrhead_join之后这些资源才会释放
        unjoinable: 这些资源线程函数退出，或者pthread_exit时自动会被释放
    2. unjoinable属性可以在pthread_create的时候指定，或者pthread_detach(pthread_self()),将状态修改为unjoinable，则线程的资源会在退出的时候(返回或者pthread_exit)自动回收
    */
    if(started_ && !joined_) //joinable的状态就是普通的工作线程，需要主线程jion工作线程的回收
    {
        thread_->detach(); //thread类提供的设置分离线程的方法，linux下底层调用的还是pthread_detach
        //thread_->detach(); 线程结束后，资源自动回收
    }
}

void Thread::start() //一个thread对象，就是记录一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // 开启线程
    thread_  = std::shared_ptr<std::thread>(new std::thread([&](){  //线程不一定马上能开启成功，所以需要通过信号量来控制
        //获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数
        func_(); 
    })); //用lambda表达式，以引用的方式接收外部对象

    // 这里必须等待上面新创建的线程的tid值, 确保线程创建成功？
    sem_wait(&sem); //如果线程没有创建成功，就会阻塞
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32]={0};
        snprintf(buf,sizeof(buf),"Thread %d",num);
        name_ = buf;
    }
}