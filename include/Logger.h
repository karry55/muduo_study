#pragma once

#include "noncopyable.h"
#include <string>

/*
================================================================================
【基础模块 2/5】Logger —— 日志
================================================================================
职责：把一行日志打到屏幕上。本项目是最简版（原版 muduo 还支持异步落盘、
      日志滚动、按大小切分文件等）。

三个设计要点（复习时重点理解"为什么用宏"）：
  1) 单例：Logger::instance() 返回全局唯一对象。写法是"函数内 static 局部变量"，
     C++11 起保证它的初始化是线程安全的（Meyers Singleton）。
  2) 为什么用【宏】而不是函数：
       - 宏能配合 __VA_ARGS__ 支持可变参数（用起来像 printf）；
       - 宏能顺手把"级别"设好（LOG_INFO 先 setLogLevel(INFO)），调用处不用关心；
       - 宏还能拿到调用位置（原版 muduo 用它拼 __FILE__/__LINE__）。
  3) 先用 snprintf 拼进 char buf[1024]，再交给 log() 输出：格式统一、逻辑集中。

用法（本库内部）：
    LOG_INFO("fd=%d events=%d\n", channel->fd(), channel->events());
    LOG_FATAL / LOG_ERROR 同理；LOG_DEBUG 需要定义 MUDEBUG 宏才生效。

【别搞混】src/main.cpp、src/chat_server.cpp 里的 LOG_INFO << "..." 是
   第三方真 muduo 的 <muduo/base/Logging.h>，和本文件这个"自己写的 Logger"不是一回事。
================================================================================
*/

// LOG_INFO("%s %d",arg1,arg2)：用法和 printf 相同
#define LOG_INFO(logmsgFormat, ...)                                            \
  do {                                                                         \
    Logger &logger = Logger::instance();                                       \
    logger.setLogLevel(INFO);                                                  \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)

// 结构同上，只是级别不同：ERROR 用于"能继续跑但不对"，FATAL 用于"必须停"
#define LOG_ERROR(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger &logger = Logger::instance();                                       \
    logger.setLogLevel(ERROR);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)

#define LOG_FATAL(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger &logger = Logger::instance();                                       \
    logger.setLogLevel(FATAL);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)

// DEBUG 级别默认被"编译掉"（空宏），性能零开销；想看调试日志就在编译时加 -DMUDEBUG
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger &logger = Logger::instance();                                       \
    logger.setLogLevel(DEBUG);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 日志级别：Logger::log 用 switch 靠这个值决定打印哪个前缀
enum LogLevel {
  INFO,  // 普通信息
  ERROR, // 错误信息
  FATAL, // core信息
  DEBUG, // 调试信息
};

// 输出一个日志类
class Logger : noncopyable {
public:
  static Logger &instance();   // 取全局唯一实例（第一次调用时构造）
  void setLogLevel(int level); // 宏在拼字符串之前调用它
  void log(std::string msg);   // 真正输出：[级别] 时间:内容

private:
  // 注意：它是"当前这条日志的级别"，每次 LOG_xxx 都会被覆盖，
  // 不是"全局过滤阈值"（原版 muduo 的 level 是用来过滤的，这里简化了）
  int logLevel_;
  Logger() {} // 构造函数私有 → 外面只能通过 instance() 拿到对象
};
