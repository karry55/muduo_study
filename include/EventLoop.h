#pragma once


//时间循环类  主要包含了两个大模块Channel Poller(epoll的抽象)
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