/*
moduo网络库给用户提供了两个主要的类
TcpServer:用于编写服务器程序的
TcpClient:用于编写客户端程序的

epoll+线程池
好处:能够把网络I/O的代码和业务代码区分开
                        用户的连接和断开      用户的可读写事件
 */

/*
================================================================================
【用法示例 2】chat_server.cpp —— 和 main.cpp 一模一样套路，只是回调里换了业务
================================================================================
复习时把这两个文件并排看，你会发现 muduo 的"使用模板"是固定的：
    EventLoop + InetAddress + TcpServer + 两个回调 + start/loop

两者唯一的区别：
    main.cpp        —— 打印连接状态 + 收到什么回什么（echo）
    chat_server.cpp —— 打印聊天内容；连接断开时调用 conn->shutdown() 主动关闭

这个文件多出来的一个知识点：setThreadNum(4)
    muduo 是 "one loop per thread" 模型：
        1 个 mainLoop（主线程，只管 accept 新连接）
        N 个 subLoop（worker 线程，每个线程一个 EventLoop，负责已建立连接上的读写）
    所以 setThreadNum(4) 大致是"1 个 I/O 线程 + 3 个 worker 线程"，
    好处是：连接的读写被分摊到多个线程/多个 epoll 上，单线程不会成为瓶颈。
================================================================================
*/

#include <functional>
#include <iostream>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <string>
using namespace std;
using namespace muduo::net;
using namespace placeholders; // 有了它就能直接写 _1 _2 _3，不用 std::placeholders::_1
using namespace muduo;

/*基于moduo网络库开发服务器程序
1.组合TcpServer对象
2.创建Eventloop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数,输出ChatServer的构造函数
4.在当前服务器类的构造函数当中,注册处理连接的回调函数和处理读写事件的回调函数
5.设置合适的服务端线程数数量,muduo库会自己分配I/O线程和worker线程
*/
class ChatServer {
public:
  ChatServer(EventLoop *loop,               // 事件循环
             const InetAddress &listenAddr, // IP+Port
             const string &nameArg)         // 服务器的名字
      : _server(loop, listenAddr, nameArg), _loop(loop) {
    // 给服务器注册用户连接的创建和断开回调
    // _1 = const TcpConnectionPtr&（连接对象）
    _server.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));

    // 给服务器注册用户读写事件回调
    // _1 连接、_2 Buffer*（收到的数据）、_3 Timestamp（收到时刻）
    _server.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置服务器端的线程数量   1个I/O线程   3个worker线程
    _server.setThreadNum(4);
  }

  void start() { _server.start(); }

private:
  // 专门处理用户的连接创建和断开   epoll   listenfd   accept
  void onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
      // 连接建立：打印 对端地址 -> 本端地址
      cout << conn->peerAddress().toIpPort() << " - >"
           << conn->localAddress().toIpPort() << "state :online" << endl;
    } else {
      cout << conn->peerAddress().toIpPort() << " - >"
           << conn->localAddress().toIpPort() << " state:offline" << endl;
      conn->shutdown(); // 主动关闭写端：close(fd) 的 muduo 封装
      // _loop->quit();  // 如果想让整个服务器退出，就调这个（会结束 loop()）
    }
  }

  // 专门处理用户的读写事件
  void onMessage(const TcpConnectionPtr &conn, // 连接
                 Buffer *buffer,               // 缓冲区
                 Timestamp time)               // 接收到的数据的时间信息
  {
    // retrieveAllAsString：把缓冲区里的数据"取走"并转成 string
    // （取走 = 数据从 Buffer 中删除，下次不会重复收到）
    string buf = buffer->retrieveAllAsString();
    cout << "recv data :" << buf << " time: " << time.toString() << endl;
    conn->send(buf); // 原样发回，构成一个最简单的"聊天（回显）服务器"
  }

  TcpServer _server; // # 1
  EventLoop *_loop;  // # 2 epoll
};


int main() {
    EventLoop loop;                        // 事件循环
    InetAddress addr("127.0.0.1",6000);    // 注意：这里只监听本机 127.0.0.1:6000
    ChatServer server(&loop,addr,"ChatServer");

    server.start();                        // 开始监听 + 启动线程池
    loop.loop();                           // 进入事件循环（阻塞）

    return 0; 
    }
