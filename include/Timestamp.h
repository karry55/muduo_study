#pragma once

#include <iostream>
#include <string>

/*
================================================================================
【基础模块 3/5】Timestamp —— 时间戳
================================================================================
职责：用一个 64 位整数表示"某个时间点"，并提供 toString() 给人看。

为什么需要它：
  - epoll_wait 的超时时间、定时器、日志时间、TcpConnection 记录"最后一次
    收到数据的时间"……都要用到时间；
  - 用整数保存，方便传值、方便相减算间隔（原版 muduo 还有 operator+/-、
    secondsSinceEpoch()、now() 用 gettimeofday 拿微秒）。

【本项目与原版 muduo 的差异 —— 复习时要能说出这个区别】
  - 原版：microSecondsSinceEpoch_ 存的确实是"微秒"；
  - 本项目：now() 用的是 time(NULL)（秒），toString() 也直接把它当秒来解析。
    也就是说成员变量的名字和实际单位对不上 —— 属于教学简化版。
================================================================================
*/

// 时间类
class Timestamp {
public:
  Timestamp();                                       // 默认构造 = 0（纪元时刻）
  explicit Timestamp(int64_t microSecondsSinceEpoch); // explicit：禁止 Timestamp t = 5; 这种隐式转换
  static Timestamp now();                             // 取当前时间
  std::string toString() const;                       // "2025-01-01 12:00:00"

private:
  int64_t microSecondsSinceEpoch_;
};
