#include "Poller.h"
#include "EPollPoller.h"

#include<stdlib.h>

// 实现在公共的源文件， 在这里添加派生的依赖关系没问题
// 通过公共文件，分裂基类和子类的强耦合，派生类可以依赖基类，但是基类不要依赖派生类
// 基类本来就是抽象的，不能依赖派生类具体的实现
Poller* Poller::newDefaultPoller(EventLoop* loop) //给EventLoop返回一个具体IO的实例
{
    if(::getenv("MUDUO_USE"))  //getenv获取环境变量，map实现的，通过key找value
    {
        return nullptr;  //生成poll的实例
    }
    else
    {

        return new EPollPoller(loop);
    }
}