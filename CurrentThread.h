#pragma once

#include<unistd.h>
#include<sys/syscall.h> //#include<unistd.h> 一起包含syscall

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()  //内联只在当前文件起作用？
    {
        if(__builtin_expect(t_cachedTid ==0,0)) ///如果t_cachedTid是0，就表示还没获取当前 线程的id
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}