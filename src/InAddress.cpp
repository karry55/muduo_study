#include "InAddress.h"

#include <cstring>
#include <iostream>
#include <string>
#include <strings.h>

// 用 "端口 + IP" 构造地址
InAddress::InAddress(uint16_t port, std::string ip) {
  // 1) 先把整个结构体清零，避免残留垃圾值（sin_zero 那几个字节必须为 0）
  bzero(&addr_, sizeof addr_);
  // 2) 指定协议族：AF_INET = IPv4
  addr_.sin_family = AF_INET;
  // 3) 端口必须转成【网络字节序】（大端）；htons = host to network short
  addr_.sin_port = htons(port);
  // 4) 字符串 IP -> 32 位整数，inet_addr 返回的已经是网络字节序
  addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

// 只要 IP 字符串
std::string InAddress::toIp() const {
  char buf[64] = "";
  // inet_ntop = network to presentation：把二进制 IP 转成 "127.0.0.1" 这种字符串
  inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  return buf;
}

// "IP:端口"，打印日志时最常用
std::string InAddress::toIpPort() const {
  char buf[64] = "";
  inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  size_t end = strlen(buf); // 记下已经写到哪，接着往后追加 ":port"
  uint16_t port = ntohs(addr_.sin_port); // ntohs = network to host short，转回本机字节序
  snprintf(buf + end, sizeof buf - end, ":%u", port);
  return buf;
}

uint16_t InAddress::toPort() const {
  return ntohs(addr_.sin_port);
}

// 独立小 main：单独编译运行 build/InAddress，验证地址封装是否正确
int main() {

  InAddress addr(8080);
  std::cout << addr.toIpPort() << std::endl;
  return 0;
}
