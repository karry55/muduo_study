#include "Channel.h"
#include "EPollPoller.h"
#include "Logger.h"
#include <errno.h>
#include <unistd.h>
#include <cstring>

/*
================================================================================
EPollPoller 的实现 —— 本项目的"引擎"
================================================================================
下面三个常量描述一个 Channel 在 epoll 里的"三态"，
它们和 Channel::index_ 一一对应（新增 Channel 时 index_ 被初始化成 -1）：

    kNew     = -1  Channel 还没被加进 epoll          → 需要 EPOLL_CTL_ADD
    kAdded   =  1  Channel 已经加进 epoll            → 事件变了用 EPOLL_CTL_MOD
    kDeleted =  2  曾经加过，后来"没兴趣"被摘掉了     → 再想用要重新 ADD
                     （注意：kDeleted 时 Channel 仍在 channels_ 里，只是不在内核的 epoll 里）

复习时把 updateChannel() 的 if/else 和这张表对照着看，就明白 state 机是怎么转的。
================================================================================
*/

// channel未添加到poller中
const int kNew = -1;
// channel已添加到epoller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

// 构造：创建 epoll 实例 + 把事件数组预分配到 16 个
// EPOLL_CLOEXEC：fork 出的子进程不继承这个 fd（避免 exec 后 fd 泄漏）
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(KInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_FATAL("epoll_create error:%d\n", errno);
  }
}
// 析构：把 epoll 实例的 fd 关掉（每创建一个 epoll 就消耗一个 fd，忘了关会泄漏）
EPollPoller::~EPollPoller() { ::close(epollfd_); }

// 填写活跃的连接
// 输入：numEvents 个有效事件（在 events_ 数组前 numEvents 个位置）
// 输出：把这些事件对应的 Channel 依次 push 进 activeChannels，并告诉它"实际发生了什么"
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    // 取出注册时存进去的 Channel*（对应 update() 里的 event.data.ptr = channel）
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    // 把内核报告的事件位写回 Channel，供 Channel::handleEventWithGuard 判断
    channel->set_revents(events_[i].events);
    // 交给 EventLoop，稍后由它逐个 channel->handleEvent()
    activeChannels->push_back(channel);
  }
}

// 更新channel通道：本函数是"状态机"，真正的 epoll_ctl 在 update() 里
void EPollPoller::updateChannel(Channel *channel) {
  const int index = channel->index();
  LOG_INFO("fd=%d events=%d  index=%d\n", channel->fd(), channel->events(),
           index);

  if (index == kNew || index == kDeleted) {
    // 情况 A：这个 Channel 目前不在内核的 epoll 里 → 一律用 ADD
    if (index == kNew) {
      int fd = channel->fd();
      channels_[fd] = channel; // kNew 时才是第一次见，需要登记到"记账本"
    }
    // kDeleted 时 channels_ 里已经有它了，不用重复登记
    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else // channel已经在poller注册过了
  {
    // 情况 B：已经在 epoll 里 → 要么"取消关注"，要么"改关注的事件"
    if (channel->isNoneEvent()) {
      // 一个事件都不关心了：从 epoll 摘掉，但保留 channels_ 里的登记（下次还能快速加回来）
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

// 真正调用 epoll_ctl 的地方
// operation: EPOLL_CTL_ADD / MOD / DEL
void EPollPoller::update(int operation, Channel *channel) {
  epoll_event event;
  memset(&event, 0, sizeof event); // 清零，避免脏数据
  int fd = channel->fd();
  event.events = channel->events(); // 关心的事件（EPOLLIN / EPOLLOUT ...）
  event.data.ptr = channel;         // 把 Channel 指针塞进去，epoll_wait 后能直接取回

  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    // 出错处理分两类：
    //   DEL 失败通常无害（比如 fd 已经被关掉了），记一条 ERROR 继续跑；
    //   ADD/MOD 失败说明程序逻辑或 fd 出大问题了，用 FATAL 直接暴露。
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl op=DEL fd=%d error:%d\n", fd, errno);
    } else {
      LOG_FATAL("epoll_ctl op=%d fd=%d error:%d\n", operation, fd, errno);
    }
  }
}

// 核心：等待事件发生，并把活跃的channel填进activeChannels
// 返回值：本次 poll 的时刻（原版 muduo 用来给 Channel::handleEvent 传 receiveTime）
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  // events_.data() 是连续内存的首地址：epoll_wait 直接把结果写进来，
  // 最多写 events_.size() 个，timeoutMs 毫秒后超时返回 0
  int numEvents = ::epoll_wait(epollfd_, events_.data(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno; // 先存下来：后面几个调用可能覆盖 errno
  Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    LOG_INFO("%d events happened\n", numEvents);
    fillActiveChannels(numEvents, activeChannels);
    // 事件数组被打满，说明很可能还有事件没取出来，容量翻倍降低下次被填满的概率
    if (numEvents == static_cast<int>(events_.size())) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents < 0 && savedErrno != EINTR) {
    // 被信号打断（EINTR）属于正常情况，忽略；其他错误才记录
    errno = savedErrno;
    LOG_ERROR("EPollPoller::poll() error:%d\n", savedErrno);
  }

  return now;
}

//从epoller中删除channel
void EPollPoller::removeChannel(Channel *channel) {
  int fd = channel->fd();
  channels_.erase(fd); // 1) 从"记账本"中删掉

  int index = channel->index();
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel); // 2) 只有真的加进过 epoll 才需要 DEL
  }
  channel->set_index(kNew); // 3) 状态复位成"未添加"，这个 Channel 对象将来还能复用
}
