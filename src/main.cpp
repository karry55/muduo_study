// myproject —— muduo 最小可用样例：TCP echo 服务器
//
// 编译：  cmake -B build -S . && cmake --build build
// 运行：  ./build/main 8888
// 测试：  printf 'hello\n' | timeout 2 nc 127.0.0.1 8888
//        （没有 nc 就用：  exec 3<>/dev/tcp/127.0.0.1/8888; echo hi >&3; cat <&3）

/*
================================================================================
【用法示例 1】main.cpp —— "怎么用 muduo 写一个 echo 服务器"
================================================================================
★ 重要：这个文件 include 的是 <muduo/net/...>，也就是 third_party/muduo 里的
  【真·muduo 库】，不是你自己写的那套 include/Channel.h！
  两者同名不同物，复习时千万别混：
      src/main.cpp、src/chat_server.cpp   → 学"怎么用库"（API 长什么样）
      src/Channel.cpp、src/EPollPooller.cpp… → 学"库内部怎么实现"

写一个 muduo 服务器的固定五步（背下来，任何 muduo 程序都长这样）：
    1. 建 EventLoop          —— 事件循环，程序的主轴
    2. 建 InetAddress        —— 监听哪个 IP:Port
    3. 建 TcpServer          —— 把 loop + 地址 + 名字交给它
    4. 设置回调              —— 连接回调 + 消息回调（你的业务逻辑都在这里）
    5. start() 然后 loop()   —— 启动监听，进入事件循环（阻塞，直到 loop 退出）
================================================================================
*/

#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>

using namespace muduo;
using namespace muduo::net;

// 把"业务"封装成一个类：组合一个 TcpServer，两个回调里写自己的逻辑
class EchoServer {
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
      : server_(loop, listenAddr, "EchoServer") { // 第 3 步：构造 TcpServer
    // 第 4 步：注册回调。
    // std::bind 把成员函数包成可调用对象；_1/_2/_3 是占位符，对应回调的参数。
    // onConnection 的参数是 TcpConnectionPtr（连接智能指针）
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
    // onMessage 的三个参数：连接、缓冲区、收到数据的时刻
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));
    // 需要多线程处理时打开这行（muduo 的 one loop per thread 模型）
    // server_.setThreadNum(4);
  }

  void start() { server_.start(); } // 第 5 步上半：开始监听

 private:
  // 连接建立/断开都会回调这里。conn->connected() 区分两种状态。
  // peerAddress() = 对端地址，localAddress() = 本端地址。
  void onConnection(const TcpConnectionPtr& conn) {
    LOG_INFO << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");
  }

  // 收到数据：从 Buffer 里把所有字节取出来，再原样发回去（echo）
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    std::string msg(buf->retrieveAllAsString()); // 取走 = 从缓冲区删除，注意只取得到一次
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at "
             << receiveTime.toFormattedString();
    conn->send(msg);  // echo 回去（send 是线程安全的，内部可能先写进 outputBuffer）
  }

  TcpServer server_;
};

int main(int argc, char* argv[]) {
  LOG_INFO << "pid = " << getpid() << ", muduo echo server";
  const int port = (argc > 1) ? std::atoi(argv[1]) : 8888; // 默认 8888

  EventLoop loop;                                       // ① 事件循环
  InetAddress listenAddr(static_cast<uint16_t>(port));   // ② 监听地址（默认 0.0.0.0）
  EchoServer server(&loop, listenAddr);                  // ③④ 建服务器并注册回调
  server.start();                                        // ⑤ 开始监听
  LOG_INFO << "listening on port " << port;
  loop.loop();                                           // 进入事件循环，阻塞在这里
  return 0;
}
