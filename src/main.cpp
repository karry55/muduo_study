// myproject —— muduo 最小可用样例：TCP echo 服务器
//
// 编译：  cmake -B build -S . && cmake --build build
// 运行：  ./build/myproject 8888
// 测试：  printf 'hello\n' | timeout 2 nc 127.0.0.1 8888
//        （没有 nc 就用：  exec 3<>/dev/tcp/127.0.0.1/8888; echo hi >&3; cat <&3）

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

class EchoServer {
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
      : server_(loop, listenAddr, "EchoServer") {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));
    // 需要多线程处理时打开这行（muduo 的 one loop per thread 模型）
    // server_.setThreadNum(4);
  }

  void start() { server_.start(); }

 private:
  void onConnection(const TcpConnectionPtr& conn) {
    LOG_INFO << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    std::string msg(buf->retrieveAllAsString());
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at "
             << receiveTime.toFormattedString();
    conn->send(msg);  // echo 回去
  }

  TcpServer server_;
};

int main(int argc, char* argv[]) {
  LOG_INFO << "pid = " << getpid() << ", muduo echo server";
  const int port = (argc > 1) ? std::atoi(argv[1]) : 8888;

  EventLoop loop;
  InetAddress listenAddr(static_cast<uint16_t>(port));
  EchoServer server(&loop, listenAddr);
  server.start();
  LOG_INFO << "listening on port " << port;
  loop.loop();
  return 0;
}
