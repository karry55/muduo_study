#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

/*
================================================================================
【基础模块 4/5】InAddress —— 套接字地址（IP + 端口）的封装
================================================================================
职责：把 C 的 struct sockaddr_in 包成一个好用的 C++ 类。

为什么需要它：
  - 服务器要告诉内核"监听哪个 IP、哪个端口"（bind）；
  - 连接建立后要能打印"对端地址 -> 本地地址"（TcpConnection 的 peerAddress/
    localAddress）；
  - sockaddr_in 里全是 C 风格细节：sin_family、网络字节序 htons、inet_addr…
    把这些藏进类里，业务代码就只用看 toIpPort() 这种直观接口。

【提示】真 muduo 里这个类叫 InetAddress（多个 t），还提供 getSockAddrLen()、
  按 "ip:port" 字符串解析的构造函数等；本项目的 InAddress 是它的子集。
================================================================================
*/

// 封装socket地址类型
class InAddress {

public:
  // explicit 禁止隐式转换；默认 ip 是本机回环地址，
  // 所以 InAddress(8080) 就表示 "127.0.0.1:8080"
  explicit InAddress(uint16_t port, std::string ip = "127.0.0.1");
  // 从内核给的地址结构直接构造（将来 accept 拿到客户端地址时会用）
  explicit InAddress(const struct sockaddr_in &addr) : addr_(addr) {};

  std::string toIp() const;     // "127.0.0.1"
  std::string toIpPort() const; // "127.0.0.1:8080"
  uint16_t toPort() const;      // 8080

  // 给 ::bind / ::accept 这类系统调用用的裸指针（C 接口只认 sockaddr*）
  const sockaddr *getSockAddr() const { return (struct sockaddr *)&addr_; }

private:
  struct sockaddr_in addr_; // 真正干活的是这个 C 结构体
};
