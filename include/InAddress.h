#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装socket地址类型
class InAddress {

public:
  explicit InAddress(uint16_t port, std::string ip = "127.0.0.1");
  explicit InAddress(const struct sockaddr_in &addr) : addr_(addr) {};

  std::string toIp() const;
  std::string toIpPort() const;
  uint16_t toPort() const;

  const sockaddr *getSockAddr() const { return (struct sockaddr *)&addr_; }

private:
  struct sockaddr_in addr_;
};