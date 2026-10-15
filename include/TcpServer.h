#pragma once

// 对外的服务器编程使用的类

/*
================================================================================
【还没写】TcpServer —— 用户唯一需要面对的"服务器类"
================================================================================
它在真 muduo 里的角色：
    把"监听 + accept + 管理所有连接 + 线程池"这些脏活全包起来，
    用户只写两个回调（连接来了干什么、收到数据干什么）。

真 muduo 的 TcpServer 内部由这几个东西组成（也是你接下来的作业清单）：
    EventLoop *loop_;               // 主线程的 loop（负责 accept）
    const string ipPort_;           // 服务名，用于日志
    const string name_;             // 服务器名字
    std::unique_ptr<Acceptor> acceptor_;            // 管 listenfd：socket/bind/listen/accept
    std::shared_ptr<EventLoopThreadPool> threadPool_;// 管理 subLoop（worker 线程）
    ConnectionMap connections_;     // 所有已建立的连接 name -> TcpConnectionPtr
    ConnectionCallback connectionCallback_;   // 连接建立/断开时的回调（用户填）
    MessageCallback messageCallback_;         // 收到消息时的回调（用户填）
    WriteCompleteCallback writeCompleteCallback_; // 数据写完的回调

对外的方法（照着写）：
    TcpServer(EventLoop *loop, const InAddress &listenAddr, const string &nameArg);
    void start();                       // 启动线程池 + acceptor_->listen()
    void setThreadNum(int numThreads);  // 设置 worker 线程数（one loop per thread）
    void setConnectionCallback(...);
    void setMessageCallback(...);
    // 内部回调：Acceptor 接受新连接 → newConnection() → 建 TcpConnection 并注册到 subLoop

【依赖关系】TcpServer → Acceptor（新类）→ Channel + EventLoop
            TcpServer → TcpConnection（新类）→ Channel + EventLoop + Buffer
也就是说：你这几个"还没写"的类，骨架都能从 Channel/EventLoop 这套已经写好的东西里长出来。
================================================================================
*/

class TcpServer {};
