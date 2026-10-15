/*
================================================================================
TcpServer 的实现 —— 【空文件，等待你写】
================================================================================
（这个文件目前没有任何代码，所以 build/libcommon.a 里没有它的实现；
  但因为也没有人调用 TcpServer，所以链接不会报错。）

动手顺序建议（每写一个类都能单独编译）：
  1. Buffer        —— 收发缓冲区（读：readv 到栈上再 append；写：直接 append 到 string）
  2. Acceptor      —— 持有 listenfd 的 Channel：构造时 socket/bind/listen，
                      setReadCallback 里循环 accept，
                      每来一个连接就调用 用户传进来的 NewConnectionCallback(connfd, peerAddr)
  3. TcpConnection —— 一个已建立的连接：Channel + Buffer(输入/输出) + 状态机
                      (kConnecting/kConnected/kDisconnecting/kDisconnected)
                      handleRead: 从 fd 读到 inputBuffer_ → 调 messageCallback_
                      send(): 先放 outputBuffer_，能直接写就直接 ::write，
                              写不完就 enableWriting()，handleWrite 里继续发
                      还涉及 shared_from_this + Channel::tie 的"生命周期魔法"
  4. EventLoopThread / EventLoopThreadPool —— 每个线程一个 EventLoop（one loop per thread）
  5. TcpServer     —— 把上面这些组装起来，对外只暴露两个回调

【学习建议】先别看 TcpServer，先把 EventLoop 写出来并让它真的转起来；
再写 Acceptor，用 nc/printf 连上验证 accept 成功；最后才做 TcpConnection 的收发。
================================================================================
*/
