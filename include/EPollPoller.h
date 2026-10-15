#pragma once

#include "Poller.h"

#include <sys/epoll.h>
#include <vector>

/*
epoll 的使用
epoll_ctl  add/mod.del
epoll_wait
*/

class EPollPoller : public Poller {
public:
  EPollPoller(EventLoop *Loop);
  ~EPollPoller() override;

  // 重写基类Poller的抽象方法
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  static const int KInitEventListSize = 16;

  // 填写活跃的连接
  void fillActiveChannels(int numEvents, ChannelList *actveChannels) const;
  
  //更新channel通道
  void update(int operation, Channel *channel);

  using EventList = std::vector<epoll_event>;

  int epollfd_;
  EventList events_;
};
