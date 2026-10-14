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
class Channel : noncopyable {
public:
  typedef std::function<void()> EventCallback;
  typedef std::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop *loop, int fd);
  ~Channel();

  // fd得到poller通知以后,处理事件,调用回调函数
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数对象
  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  void tie(const std::shared_ptr<void> &);

  int fd() const { return fd_; }
  void set_revents(int revt) { revents_ = revt; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  // 设置fd相应的事件状态
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
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
    events_ = kNoneEvent;
    update();
  }

  // 返回fd当前的事件状态
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  int index() const { return index_; }

  // one loop per thread
  EventLoop *ownerLoop() { return loop_; }
  void remove();

private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_; // 事件循环
  const int fd_;    // fd,Poller监听的对象
  int events_;      // 注册fd感兴趣的事件
  int revents_;     // Poller返回的具体发生的事件
  int index_;       // 表示Channel在Poller中的状态

  std::weak_ptr<void> tie_;
  bool tied_;

  // 因为channel通道里面能够获知fd最终发生的具体事件revents,所以它负责调用具体的回调操作
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};