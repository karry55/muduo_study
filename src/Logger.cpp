#include "Logger.h"
#include "Timestamp.h"
#include <iostream>
#include <string>
#include <time.h>

// 获取日志唯一的实例对象
// 函数内的 static 局部变量：第一次调用时构造，程序退出时析构；
// C++11 起这个初始化过程是线程安全的，所以不需要额外加锁。
Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

// 设置日志级别
// 由 LOG_INFO/LOG_ERROR/LOG_FATAL 这三个宏在调用 log() 之前设置，
// 所以 log() 里 switch(logLevel_) 拿到的就是"本条日志"的级别。
void Logger::setLogLevel(int level) { logLevel_ = level; }

// 写日志  [级别信息] time:msg
void Logger::log(std::string msg) {
  // 1) 先按级别打印前缀
  switch (logLevel_) {
  case INFO:
    std::cout << "[INFO]";
    break;
  case ERROR:
    std::cout << "[ERROR]";
    break;
  case FATAL:
    std::cout << "[FATAL]";
    break;
  case DEBUG:
    std::cout << "[DEBUG]";
    break;
  default:
    break;
  }

  // 2) 再打时间 + 内容。Timestamp::now().toString() 得到 "2025-01-01 12:00:00"
  //    这里依赖 Timestamp 模块，所以 Logger 的位置排在 Timestamp 之后看更顺。
  std::cout << Timestamp::now().toString() << ":" << msg << std::endl;
}
