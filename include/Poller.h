#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;

/*
================================================================================
【核心模块 2/3】Poller —— IO 复用的"抽象基类"（多态接口）
================================================================================
一句话职责：
    定义"所有 IO 复用机制都必须提供的能力"，至于底层是 epoll、poll 还是 select，
    由派生类（本项目只有 EPollPoller）决定。

这就是"面向接口编程"：
    EventLoop 只认识 Poller*，从不 include <sys/epoll.h>；
    想换成 poll 实现，只要再写一个 PollPoller，EventLoop 一行都不用改。
    （真 muduo 里就有 EPollPoller 和 PollPoller 两套。）

三个纯虚函数（=0，派生类必须实现）：
    poll()          等事件，把活跃的 Channel 填进 activeChannels
    updateChannel() 新增/修改某个 Channel 的关注事件
    removeChannel() 把某个 Channel 从 IO 复用里摘掉

一个静态工厂：
    newDefaultPoller(loop)  →  根据环境返回具体实现（本项目的实现见 DefaultPoller.cpp）

一个"记账本"：
    channels_  unordered_map<fd, Channel*>，Poller 靠它知道"这个 fd 归哪个 Channel 管"，
               也用于 hasChannel() 做一致性校验。
================================================================================
*/

class Poller : noncopyable {
public:
  // 类型别名：活跃 Channel 的列表。poll() 的第二个参数就是它（出参）
  // 用"指针的 vector"而不是"Channel 的 vector"，避免拷贝 Channel（它不可拷贝）
  using ChannelList = std::vector<Channel *>;

  Poller(EventLoop *loop);
  virtual ~Poller() = default; // 基类析构必须是 virtual，否则 delete 基类指针不会调派生类析构

  //给所有IO复用保留统一的接口
  virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels)=0;
  virtual void updateChannel(Channel *channel)=0;
  virtual void removeChannel (Channel *channel)=0;

  //判断参数channel是否在当前Poller中
  bool hasChannel(Channel *channel) const;

  //EventLoop可以通过该接口获取默认的IO复用的具体实现
  static Poller * newDefaultPoller(EventLoop *loop);
protected:

//map的key:sockfd value:sockfd所属的channel通道类型
  // protected：派生类要直接操作它（EPollPoller 里 channels_[fd] = channel、channels_.erase(fd)）
  using ChannelMap = std::unordered_map<int, Channel *>;
  ChannelMap channels_;

private:
  EventLoop *owerLoop_;  //定义Poller所属的事件循环EventLoop
                         //（注意：原版 muduo 拼作 ownerLoop_，这里少了个 n，不影响功能）
};
