#include"EventLoop.h"

/*
================================================================================
EventLoop 的实现 —— 【待你完成】
================================================================================
这个文件现在是空的。照着下面的步骤写，写完你的库就能真正跑起来了。

建议的实现顺序（每一步都能单独编译过）：

--- 第 1 步：补头文件里的成员 ---
在 include/EventLoop.h 的 private: 区域加上：
    std::unique_ptr<Poller> poller_;
    std::vector<Channel *> activeChannels_;
    bool quit_;
    const pid_t threadId_;
    static const int kPollTimeMs = 10000;
并在文件顶部 #include <memory> <vector> <unistd.h>。

--- 第 2 步：构造函数 ---
EventLoop::EventLoop() : quit_(false), threadId_(::gettid()) {
  LOG_INFO("EventLoop created %d\n", threadId_);
  poller_.reset(Poller::newDefaultPoller(this));   // ★ 这里拿到 EPollPoller
}
（注意：DefaultPoller.cpp 现在 return nullptr，所以第 3 步要先把那里改成
  return new EPollPoller(loop); 否则这里会拿到空指针。）

--- 第 3 步：转发两个注册函数 ---
void EventLoop::updateChannel(Channel *channel) { poller_->updateChannel(channel); }
void EventLoop::removeChannel(Channel *channel) { poller_->removeChannel(channel); }

--- 第 4 步：事件循环本体（最重要的 10 行）---
void EventLoop::loop() {
  quit_ = false;
  while (!quit_) {
    activeChannels_.clear();                       // 复用容器，避免每轮都分配内存
    poller_->poll(kPollTimeMs, &activeChannels_);  // 阻塞等待，超时 10s
    for (Channel *channel : activeChannels_) {
      channel->handleEvent(Timestamp::now());      // 分发到用户回调
    }
  }
}

--- 第 5 步：quit() ---
void EventLoop::quit() { quit_ = true; }
（严谨版本：如果 loop 正在 epoll_wait 里阻塞，光置标志位是叫不醒它的，
  原版 muduo 用一个 eventfd + wakeup() 来打断 epoll_wait。等你把主流程跑通再补。）

--- 写完后的验证方法 ---
// 临时在 main 里试：
EventLoop loop;
loop.loop();          // 应该一直阻塞在 epoll_wait（没有事件时安静等待）
再用 gdb/printf 确认 poller_ 不是空指针、threadId_ 正常。
================================================================================
*/
