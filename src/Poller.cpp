#include"Poller.h"
#include"Channel.h"

/*
================================================================================
Poller 的实现：注意这里【没有】任何 epoll 代码
================================================================================
这正是抽象基类的价值 —— 基类只写"所有 IO 复用都通用"的逻辑，
平台相关的部分全部留给派生类 EPollPoller。
================================================================================
*/

// 保存所属的 EventLoop 指针（现在还没被用到，将来断线程检查会用它）
Poller::Poller(EventLoop *loop):owerLoop_(loop){

}

// 判断某个 Channel 是否登记在我这里
// 两个条件缺一不可：
//   1) fd 在 map 里（it != end）
//   2) 并且对应的 Channel 指针就是它（防止 fd 被关闭后复用、换成了新 Channel 的情况）
bool Poller::hasChannel(Channel *channel)const
{

    auto it=channels_.find(channel->fd());
    return it!=channels_.end() && it->second==channel;
}
