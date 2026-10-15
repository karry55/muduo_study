#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

/*
================================================================================
Channel 的实现（对照头文件里的说明看）
================================================================================
三个静态常量：就是 epoll 的事件位
    kNoneEvent  0                      什么都不关心
    kReadEvent  EPOLLIN | EPOLLPRI     可读（含带外/紧急数据）
    kWriteEvent EPOLLOUT               可写
注意 EPOLLERR / EPOLLHUP 不需要注册，内核总会上报，所以这里没有它们。
================================================================================
*/

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// 构造：index_ 必须是 -1（对应 EPollPoller.cpp 里的 kNew），
// 表示"这个 Channel 还没被加进 epoll"。Poller 就是靠这个值判断要不要 EPOLL_CTL_ADD。
Channel::Channel(EventLoop* loop, int fd)
  : loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false)
{
}

Channel::~Channel() = default; // 这里不 close(fd_)：fd 的生命周期归它真正的拥有者管

// 绑定一个 shared_ptr 的"弱引用"，用来判断对象是否还活着
void Channel::tie(const std::shared_ptr<void>& obj)
{
  tie_ = obj;      // weak_ptr：不增加引用计数，不会阻止对象被销毁
  tied_ = true;
}

// 把"我的兴趣事件变了"这件事报给 EventLoop（EventLoop 再转给 Poller）
void Channel::update()
{
  loop_->updateChannel(this);
}

// 我要下线了（比如连接关闭），把自己从 Poller 中删除
void Channel::remove()
{
  loop_->removeChannel(this);
}

// 事件总入口：Poller 在 poll() 里发现这个 fd 活跃，就会调用它
void Channel::handleEvent(Timestamp receiveTime)
{
  // 如果 tie 过，就先尝试把 weak_ptr 提升成 shared_ptr：
  //   - 提升成功 → 对象还活着，可以安全处理事件
  //   - 提升失败 → 对象已经析构了，直接什么都不做（这就是"防止回调到已销毁对象"的经典手法，
  //                在 TcpConnection 里用来解决"连接已断开但事件还在队列里"的竞态）
  if (tied_) {
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

// 真正的分发逻辑：按 revents_（内核报上来的事件位）调用对应回调
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  LOG_INFO("channel handleEvent revents:%d", revents_);

  // 1) 对端挂断（EPOLLHUP）且没有新数据可读 —— 典型场景是客户端断开连接。
  //    这里条件写成 (EPOLLHUP && !EPOLLIN) 是因为：如果同时还有可读数据，
  //    应该先让上层把数据处理完，再走关闭流程。
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) {   // 回调可能为空，调用前必须判断，否则 std::function 会抛异常
      closeCallback_();
    }
  }
  // 2) 出错（EPOLLERR 也是内核自动上报的，不需要注册）
  if (revents_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }
  // 3) 可读：EPOLLPRI 是带外数据，一并当作可读处理
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) {
      readCallback_(receiveTime); // 把"收到的时刻"传进去，方便上层打日志/做超时统计
    }
  }
  // 4) 可写：通常意味着"发送缓冲区有空位，可以继续发数据了"
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}
