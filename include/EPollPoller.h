#pragma once

#include "Poller.h"

#include <sys/epoll.h>
#include <vector>

/*
epoll 的使用
epoll_ctl  add/mod.del
epoll_wait
*/

/*
================================================================================
【核心模块 3/3】EPollPoller —— Poller 的 epoll 实现
================================================================================
职责：把 Poller 的三个纯虚函数用 Linux epoll 实现出来。

epoll 就三个系统调用（务必记住）：
    epoll_create1()            创建 epoll 实例，返回一个 fd（epollfd_）
    epoll_ctl(epfd, op, fd, e) 增/改/删 关心的 fd
                               op = EPOLL_CTL_ADD / EPOLL_CTL_MOD / EPOLL_CTL_DEL
    epoll_wait(epfd, evs, n, timeout)  等待事件，返回活跃的个数

为什么 events_ 是 vector 而不是数组：
    epoll_wait 一次最多返回 events_.size() 个事件；
    如果这次"装满了"，说明可能还有更多事件没取出来，就把容量翻倍（见 poll() 实现）。
    初始容量 KInitEventListSize = 16（原版 muduo 叫 kInitEventListSize）。

data.ptr 与 data.fd 的区别（重要）：
    epoll_event.data 是个 union（同一块内存的不同解读方式）：
        event.data.fd = 5;              // 存 fd
        event.data.ptr = channel;       // 存指针
    两者只能留一个！本项目统一用 data.ptr 存 Channel*，
    这样 epoll_wait 回来后能直接拿到 Channel 对象（见 fillActiveChannels）。
================================================================================
*/

class EPollPoller : public Poller {
public:
  EPollPoller(EventLoop *Loop);
  ~EPollPoller() override; // override：让编译器检查"确实覆盖了基类虚函数"，写错名字会报错

  // 重写基类Poller的抽象方法
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  static const int KInitEventListSize = 16; // events_ 的初始容量

  // 填写活跃的连接
  // poll() 拿到 numEvents 个活跃事件后，由它把对应的 Channel 填进 activeChannels
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

  //更新channel通道
  // 真正调用 epoll_ctl 的地方（updateChannel 只是做状态判断后调用它）
  void update(int operation, Channel *channel);

  using EventList = std::vector<epoll_event>;

  int epollfd_;    // epoll 实例的 fd，由 epoll_create1 创建，析构时 close
  EventList events_; // 存放 epoll_wait 返回的事件，容量不够会翻倍
};
