#include "Poller.h"
#include<stdlib.h>

/*
================================================================================
【工厂】newDefaultPoller —— 决定 EventLoop 用哪个 IO 复用实现
================================================================================
调用关系：EventLoop 构造函数里会调用 Poller::newDefaultPoller(this)，
拿到一个具体实现（本应是 new EPollPoller(loop)），之后所有 poll/update 都走它。

为什么要用工厂而不是在 EventLoop 里直接 new EPollPoller：
    EventLoop 属于"上层逻辑"，不应该 include <sys/epoll.h>；
    工厂把"选哪个实现"这件事集中在一处，将来加 PollPoller（poll 实现）
    只改这里，EventLoop 一行都不用动。

为什么要有 MUDUO_USE_POLL 环境变量：
    真 muduo 用它来切换 epoll / poll 两套实现，便于对比和排错。

【待你完成】现在两个分支都 return nullptr —— 属于占位代码（能编译，但一调用
就会拿到空指针然后崩）。正确写法大致是：
    if (::getenv("MUDUO_USE_POLL")) return new PollPoller(loop);
    else                                return new EPollPoller(loop);
（需要 #include "EPollPoller.h"；用完 delete 的责任在 EventLoop 的析构里。）
================================================================================
*/

Poller* Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr; // TODO: 应该返回 new PollPoller(loop)
    }
    else{
        return nullptr; // TODO: 应该返回 new EPollPoller(loop)
    }
}
