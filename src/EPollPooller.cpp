#include "Channel.h"
#include "EPollPoller.h"
#include "Logger.h"
#include <errno.h>
#include <unistd.h>
#include <cstring>

// channel未添加到poller中
const int kNew = -1;
// channel已添加到epoller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(KInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_FATAL("epoll_create error:%d\n", errno);
  }
}
EPollPoller::~EPollPoller() { ::close(epollfd_); }

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

// 更新channel通道
void EPollPoller::updateChannel(Channel *channel) {
  const int index = channel->index();
  LOG_INFO("fd=%d events=%d  index=%d\n", channel->fd(), channel->events(),
           index);

  if (index == kNew || index == kDeleted) {
    if (index == kNew) {
      int fd = channel->fd();
      channels_[fd] = channel;
    }
    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else // channel已经在poller注册过了
  {
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EPollPoller::update(int operation, Channel *channel) {
  epoll_event event;
  memset(&event, 0, sizeof event);
  int fd = channel->fd();
  event.events = channel->events();
  event.data.ptr = channel;

  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl op=DEL fd=%d error:%d\n", fd, errno);
    } else {
      LOG_FATAL("epoll_ctl op=%d fd=%d error:%d\n", operation, fd, errno);
    }
  }
}

// 核心：等待事件发生，并把活跃的channel填进activeChannels
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  int numEvents = ::epoll_wait(epollfd_, events_.data(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    LOG_INFO("%d events happened\n", numEvents);
    fillActiveChannels(numEvents, activeChannels);
    // 事件数组被打满就扩容，否则下次还是只能拿这么多
    if (numEvents == static_cast<int>(events_.size())) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents < 0 && savedErrno != EINTR) {
    errno = savedErrno;
    LOG_ERROR("EPollPoller::poll() error:%d\n", savedErrno);
  }

  return now;
}

//从epoller中删除channel
void EPollPoller::removeChannel(Channel *channel) {
  int fd = channel->fd();
  channels_.erase(fd);

  int index = channel->index();
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}
