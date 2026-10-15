#include "Timestamp.h"

#include <cstdio> // snprintf
#include <time.h>

// 默认构造：0 时刻（原版 muduo 里 Timestamp() 也是"无效/零点"的意思）
Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

// 当前时间。
// 原版 muduo：::gettimeofday(&tv) 拿"秒+微秒"，再拼成微秒；
// 本项目简化为 time(NULL)，只精确到秒。
Timestamp Timestamp::now() {
  return Timestamp(time(NULL));
}

std::string Timestamp::toString() const {
  char buf[128] = {0};
  // localtime 返回的是"指向静态内存的指针"，多线程同时调用会互相覆盖；
  // 原版 muduo 用 Timestamp::toString 里加缓存 + __thread 来规避。单线程学习阶段够用。
  tm *tm_time = localtime((time_t *)&microSecondsSinceEpoch_);
  snprintf(buf, 128, "%4d-%02d-%02d %02d:%02d:%02d",
           tm_time->tm_year + 1900, // tm_year 是从 1900 年起算的偏移量
           tm_time->tm_mon + 1,     // tm_mon 是 0~11，所以要 +1
           tm_time->tm_mday, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);
  return std::string(buf);
}

// 这个小 main 只是为了能单独编译运行（CMakeLists 靠"文件里有没有 main"
// 把它识别成独立可执行文件 build/Timestamp），方便你自己动手实验。
int main() {
  std::cout << Timestamp::now().toString() << std::endl;
  return 0;
}
