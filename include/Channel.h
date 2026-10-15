#pragma once

#include "noncopyable.h"

#include "EventLoop.h"
#include <functional>
#include <memory>


#include "Timestamp.h"
/*
Channel 理解为通道,封装了sockfd和其感兴趣的event,如:EPOLLIN,EPOLLOUT事件
还绑定了poller返回的具体事件
*/

/*
================================================================================
【核心模块 1/3】Channel —— "一个 fd + 它关心的事件 + 出事时该调谁"
================================================================================
一句话职责：
    Channel 是【fd 与事件回调的绑定表】。它自己不做 IO，只负责：
      1) 记住这个 fd 关心哪些事件（events_：EPOLLIN / EPOLLOUT）；
      2) 记住 Poller 告诉它"实际发生了什么事件"（revents_）；
      3) 事件发生后，按 revents_ 去调用用户注册的 4 个回调之一。

它在数据流里的位置（背下来这张图，muduo 就懂一半了）：
    EventLoop::loop()
      -> Poller::poll()              // epoll_wait，返回一堆活跃 Channel
      -> channel->handleEvent(t)     // 逐个分发
      -> handleEventWithGuard(t)     // 判断 revents_，调用下面某个回调
      -> readCallback_ / writeCallback_ / closeCallback_ / errorCallback_
      -> 用户代码（比如 TcpConnection::handleRead -> 你的 onMessage）

四个成员变量的分工（最容易混）：
    fd_       监听的套接字
    events_   我"想"让 epoll 通知我什么（由 enableReading() 等设置）
    revents_  epoll 实际"告诉"我发生了什么（由 poller 用 set_revents 写入）
    index_    本 Channel 在 Poller 里的状态：kNew(-1)/kAdded(1)/kDeleted(2)

为什么要继承 noncopyable：
    Channel 内部保存了 loop_ 指针和各种回调，被拷贝就会出现"两份状态、
    析构两次"，所以禁止拷贝。

为什么 update()/remove() 只是转发：
    Channel 自己不该知道 epoll 的存在，它只对 EventLoop 说"我变了/我下线了"，
    由 EventLoop 再转给 Poller —— 这是"分层"，也是 muduo 能换 IO 复用
    （epoll / poll）的原因。
================================================================================
*/

class Channel : noncopyable {
public:
  // 两种回调类型：
  //   EventCallback     无参，用于 可写/关闭/错误
  //   ReadEventCallback 带一个 Timestamp（"什么时候收到的"），用于 可读
  // 用 std::function 是为了支持 lambda、成员函数 bind 等任意可调用对象
  typedef std::function<void()> EventCallback;
  typedef std::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop *loop, int fd);
  ~Channel();

  // fd得到poller通知以后,处理事件,调用回调函数
  // 这是 epoll 事件进入用户代码的"总入口"
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数对象
  // std::move：把传入的 function 对象"搬"进来，避免拷贝
  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  // 与 weak_ptr 配合的"生命周期保护"，详见 handleEvent() 的实现
  void tie(const std::shared_ptr<void> &);

  int fd() const { return fd_; }                        // 我管的是哪个 fd
  void set_revents(int revt) { revents_ = revt; }       // 由 Poller 调用：告诉你实际发生了什么
  int events() const { return events_; }                // 由 Poller 读取：你关心什么

  bool isNoneEvent() const { return events_ == kNoneEvent; } // 一个事件都不关心 → 该从 epoll 摘掉

  // 设置fd相应的事件状态
  // 每个 enable/disable 最后都会调用 update()，把变化同步给 Poller，
  // 所以"改兴趣事件"这动作会立刻反映到内核的 epoll 里。
  void enableReading() {
    events_ |= kReadEvent;   // 按位或：加上"可读"
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;  // 按位与非：去掉"可读"，其余位保持
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;    // 清零 = 什么都不关心
    update();
  }

  // 返回fd当前的事件状态
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  int index() const { return index_; }
  // Poller 用它标记 channel 的状态：kNew / kAdded / kDeleted（见 EPollPoller.cpp 的 updateChannel）
  void set_index(int idx) { index_ = idx; }

  // one loop per thread
  EventLoop *ownerLoop() { return loop_; } // 我属于哪个事件循环
  void remove();                           // 把自己从 Poller 里摘掉（转发给 EventLoop）

private:
  // 转发给 EventLoop -> Poller，真正的 epoll_ctl 发生在 EPollPoller::update 里
  void update();
  // handleEvent 的"真正干活"版本：只在对象还活着（tie 成功）时才被调用
  void handleEventWithGuard(Timestamp receiveTime);

  // 这三个是"常量"，用 static const 放在类里声明，定义在 Channel.cpp
  // kReadEvent = EPOLLIN | EPOLLPRI，kWriteEvent = EPOLLOUT
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_; // 事件循环
  const int fd_;    // fd,Poller监听的对象
  int events_;      // 注册fd感兴趣的事件
  int revents_;     // Poller返回的具体发生的事件
  int index_;       // 表示Channel在Poller中的状态

  // tie_/tied_ 是"防止对象被提前销毁"的保险，详见 handleEvent()
  std::weak_ptr<void> tie_;
  bool tied_;

  // 因为channel通道里面能够获知fd最终发生的具体事件revents,所以它负责调用具体的回调操作
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
