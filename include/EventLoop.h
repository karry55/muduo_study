#pragma once


//时间循环类  主要包含了两个大模块Channel Poller(epoll的抽象)

/*
================================================================================
【核心模块 4/4】EventLoop —— 事件循环（整个库的"心脏"）
================================================================================
一句话职责：
    不停地问 Poller "有事吗？" → 有事就把活跃 Channel 挨个 handleEvent()。
    它同时是"线程边界"：一个 EventLoop 属于一个线程（one loop per thread）。

为什么它在 Channel 和 Poller 中间：
    Channel 只会喊"我变了"（update/remove），Poller 只会"等事件"（poll），
    把它们拼成一个闭环的正是 EventLoop：
        Channel::update()  → EventLoop::updateChannel() → Poller::updateChannel()
        EventLoop::loop()  → Poller::poll() → Channel::handleEvent() → 用户回调

【你现在缺的实现】本类目前只有声明，没有实现（src/EventLoop.cpp 是空的）。
按下面的路线补，顺序就是从易到难：

 1) 成员变量（补在 private: 里）
      std::unique_ptr<Poller> poller_;      // 由 newDefaultPoller(this) 创建
      bool quit_;                           // loop() 的退出标志
      const pid_t threadId_;                // 本 loop 属于哪个线程（构造时记录）
      static const int kPollTimeMs = 10000; // epoll_wait 的超时（原版 10 秒）

 2) 构造函数 EventLoop::EventLoop()
      quit_ = false;
      threadId_ = CurrentThread::tid();   // 本项目可以先用 ::gettid()
      poller_.reset(Poller::newDefaultPoller(this));  // ★ 关键：拿到 EPollPoller

 3) 析构函数：什么都不用做（unique_ptr 会删掉 poller_）

 4) updateChannel / removeChannel：一行转发
      poller_->updateChannel(channel);
      poller_->removeChannel(channel);

 5) loop()（核心，建议照抄下面的骨架再自己写一遍）
      while (!quit_) {
        activeChannels_.clear();
        poller_->poll(kPollTimeMs, &activeChannels_);      // 等事件（阻塞在这里）
        for (Channel *c : activeChannels_) c->handleEvent(Timestamp::now());
      }

 6) quit()：置 quit_ = true（严格版本还要用 eventfd 唤醒 loop，见原版 muduo）

【自测】写完以后你应该能回答：
    - 为什么 loop() 里要 clear() 而不是重新构造 activeChannels_？（避免反复分配）
    - 一个线程能不能跑两个 EventLoop？为什么？（可以但不推荐，one loop per thread 的意义）
    - updateChannel 是由哪个线程调用的？（通常是"别的线程"，这正是原版要做
      wakeup 唤醒的原因 —— 跨线程操作 epoll 需要唤醒等待中的 loop）
================================================================================
*/

class Channel;   // 前置声明：下面两个函数的参数是 Channel*

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Channel 通过这两个接口把自己注册/注销到 Poller（由 EventLoop 转发给 Poller）
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);

private:
};
