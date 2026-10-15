# muduo 复习笔记（基于你自己的这套实现）

> 配套文件：[目录说明.md](目录说明.md) 讲"哪些文件是你的"；本文件讲"这些文件怎么串起来、该按什么顺序看"。
> 代码里我已经把讲解写进了注释，本文件是那份注释的"总纲"。

---

## 0. 先分清：这个工程里有"两套东西"

这是最容易把自己绕晕的地方。

| | 文件 | 你在这里学什么 | 依赖谁 |
|---|---|---|---|
| **A. 用库** | `src/main.cpp`、`src/chat_server.cpp` | muduo 的 **API 长什么样**、服务器程序的标准写法 | `third_party/muduo`（真·muduo） |
| **B. 造轮子** | `include/*.h`、`src/*.cpp`（除上面两个） | muduo **内部是怎么实现的** | 你自己写的类 |

A 里的 `#include <muduo/net/TcpServer.h>` 是**真库**；B 里的 `#include "TcpServer.h"` 是**你自己那个空壳**。
同名不同物，看代码前先确认自己在哪一套里。

---

## 1. 模块地图（谁调用谁）

```
                 用户代码
        main.cpp / chat_server.cpp          ← A. 用库（真 muduo）
                    │
     ┌──────────────┴───────────────┐
     │        B. 你自己的实现        │
     └──────────────┬───────────────┘
                    │
              TcpServer            ← 【还没写】对外只暴露两个回调
                    │
      ┌─────────────┼──────────────┐
      │             │              │
  Acceptor    TcpConnection    EventLoopThreadPool   ← 【都还没写】
      │             │
      └──────┬──────┘
             │  每个 fd 一个 Channel
          Channel  ──────────────┐
             │ update()/remove() │  handleEvent()
             ▼                    ▲
         EventLoop ───────────────┘      ← 【只有声明，实现待写】心脏
             │ updateChannel()/removeChannel()      loop() → poll()
             ▼
          Poller（抽象基类，纯虚函数）
             │ 继承
        EPollPoller（epoll 实现）      ← 引擎
             ▲
             └── newDefaultPoller() 工厂（DefaultPoller.cpp）

  基础支撑（谁都可以用）：
      noncopyable   Logger   Timestamp   InAddress
```

**依赖方向是单向的**：上层认识下层，下层不认识上层。
`EPollPoller.cpp` 里 include 了 `Channel.h`（因为它要用 `channel->fd()/events()`），
但 `Channel.h` 里**绝不** include `EPollPoller.h` —— 这是分层的关键。

---

## 2. 推荐阅读顺序（从哪个文件开始看）

**从最简单、最独立的开始，最后看最复杂的。** 具体顺序：

| # | 文件 | 预计时间 | 看完要能回答 |
|---|---|---|---|
| 1 | [include/noncopyable.h](include/noncopyable.h) | 5 分钟 | 为什么资源类必须禁止拷贝？`= delete` 和 `protected 构造` 各起什么作用？ |
| 2 | [include/Logger.h](include/Logger.h) → [src/Logger.cpp](src/Logger.cpp) | 15 分钟 | 为什么用**宏**而不是函数？单例为什么写在函数里？`logLevel_` 是"当前级别"还是"过滤阈值"？ |
| 3 | [include/Timestamp.h](include/Timestamp.h) → [src/Timestamp.cpp](src/Timestamp.cpp) | 10 分钟 | `int64_t` 存时间有什么好处？为什么 `tm_year+1900`、`tm_mon+1`？它和原版 muduo 差在哪？ |
| 4 | [include/InAddress.h](include/InAddress.h) → [src/InAddress.cpp](src/InAddress.cpp) | 15 分钟 | `htons/ntohs` 为什么必需？`inet_addr` 和 `inet_ntop` 分别干什么？ |
| 5 | **[include/Channel.h](include/Channel.h) → [src/Channel.cpp](src/Channel.cpp)** | 40 分钟 | `events_` 和 `revents_` 的区别？`index_` 是干什么的？`handleEventWithGuard` 里四个 `if` 的顺序为什么这样排？**（核心 1）** |
| 6 | [include/Poller.h](include/Poller.h) → [src/Poller.cpp](src/Poller.cpp) | 15 分钟 | 为什么要有抽象基类？`channels_` 这张表是干什么的？ |
| 7 | **[include/EPollPoller.h](include/EPollPoller.h) → [src/EPollPooller.cpp](src/EPollPooller.cpp)** | 40 分钟 | epoll 三个系统调用各干什么？`kNew/kAdded/kDeleted` 三态怎么转？`data.ptr` 为什么要存 Channel 指针？**（核心 2）** |
| 8 | [src/DefaultPoller.cpp](src/DefaultPoller.cpp) | 5 分钟 | 工厂模式在这里解决了什么问题？（就一个函数） |
| 9 | **[include/EventLoop.h](include/EventLoop.h) → [src/EventLoop.cpp](src/EventLoop.cpp)** | 30 分钟 | `loop()` 里那 5 行是怎么把 Poller 和 Channel 缝起来的？**（核心 3，待你实现）** |
| 10 | [src/main.cpp](src/main.cpp) / [src/chat_server.cpp](src/chat_server.cpp) | 20 分钟 | 写一个 muduo 服务器的"固定五步"是哪五步？两个回调的参数各是什么？ |
| 11 | [include/TcpServer.h](include/TcpServer.h) | 10 分钟 | TcpServer 内部需要哪些成员？（这是你的下一份作业） |

**一句话记住顺序**：`基础设施(Logger/Timestamp/InAddress)` → `Channel（一个 fd）` → `Poller/EPollPoller（一堆 fd）` → `EventLoop（把前两者缝起来）` → `TcpServer/TcpConnection（业务封装）`。

---

## 3. 一次"可读事件"的完整旅程（最重要，务必背下来）

假设客户端发来一段数据，从内核到你的回调，会依次经过：

```
 1. 内核把 fd 标记为可读
 2. EventLoop::loop()                         ← 一直在 while 里转
 3.   Poller::poll(timeoutMs, &activeChannels) 等待
 4.     EPollPoller::poll() → ::epoll_wait(...)   ← 阻塞解除，返回 numEvents
 5.     EPollPoller::fillActiveChannels()
 6.       Channel *c = event.data.ptr;            ← 取回当初存进去的 Channel*
 7.       c->set_revents(event.events);           ← 把"实际发生了什么"写进 Channel
 8.       activeChannels.push_back(c);
 9.   回到 loop()，遍历 activeChannels
10.   Channel::handleEvent(Timestamp::now())
11.     若 tie 过 → weak_ptr.lock() 成功才继续（对象还活着）
12.     Channel::handleEventWithGuard()
13.       命中 `if (revents_ & (EPOLLIN|EPOLLPRI))` → 调 readCallback_()
14.         用户回调：TcpConnection::handleRead() → messageCallback_ → 你的 onMessage()
```

对应的"动作流"（用户主动改事件时，方向是反的）：

```
Channel::enableReading() → events_ |= EPOLLIN → Channel::update()
   → EventLoop::updateChannel(this) → Poller::updateChannel()
   → EPollPoller::updateChannel()（查 index_ 三态）→ EPollPoller::update()
   → ::epoll_ctl(EPOLL_CTL_ADD/MOD/DEL)
```

**把这两条链路记住，muduo 内部就没有黑盒了。**

---

## 4. 模块速查表

| 模块 | 一句话职责 | 关键成员 / 函数 | 常见坑 |
|---|---|---|---|
| `noncopyable` | 禁止拷贝的基类 | `= delete` 的拷贝构造/赋值 | 忘记继承它 → 对象被意外拷贝，double free |
| `Logger` | 打日志 | `instance()`、宏 `LOG_INFO/ERROR/FATAL` | 和真库的 `LOG_INFO <<` 混用；宏里没有分号 |
| `Timestamp` | 时间点 | `now()`、`toString()` | 本项目 `now()` 只精确到秒，成员名却叫 microSeconds |
| `InAddress` | IP+端口的封装 | `toIpPort()`、`getSockAddr()` | 忘了 `htons` → 端口变成 0 或奇怪值 |
| `Channel` | **一个 fd + 关心的事件 + 回调** | `events_`/`revents_`/`index_`、`handleEvent()` | 混淆 `events_`（想关心）和 `revents_`（实际发生） |
| `Poller` | IO 复用的抽象接口 | 3 个纯虚函数、`channels_` | 析构忘了 `= default`/`virtual` → 链接错误 |
| `EPollPoller` | epoll 实现 | `epollfd_`、`events_`、三态转换 | 该 `DEL` 写成 `ADD`；`data.fd` 和 `data.ptr` 是 union，只能留一个 |
| `DefaultPoller` | 选实现的工厂 | `newDefaultPoller()` | 现在返回 `nullptr`，必须先补上 |
| `EventLoop` | **事件循环（心脏）** | `loop()`、`updateChannel()` | 还在阻塞时想退出 loop，只置标志位叫不醒它 |
| `TcpServer` | 对外的服务器类 | 两个回调、`setThreadNum()` | 还没写 |
| `Buffer`/`TcpConnection`/`Acceptor` | 连接与缓冲 | —— | 还没写 |

---

## 5. 四个必须理解的设计点

### 5.1 one loop per thread
一个线程一个 `EventLoop`，一个 `EventLoop` 一个 `EPollPoller`。
`TcpServer::setThreadNum(4)` 之后：主线程的 mainLoop 只管 `accept`，
新连接被分给某个 subLoop（worker 线程）去做读写。
**好处**：连接的读写被摊到多个 epoll 上；**代价**：跨线程操作要加锁/唤醒（原版的 `wakeup()`）。

### 5.2 Channel 的"三态"与 index_
`index_` 就是 Channel 在 Poller 里的状态标签，取值只有三个：

| 值 | 名字 | 含义 | 该做的 epoll 操作 |
|---|---|---|---|
| `-1` | `kNew` | 还没加进 epoll（刚构造，Channel.cpp 里初始化 `index_(-1)`） | `EPOLL_CTL_ADD` |
| `1` | `kAdded` | 已在 epoll 里 | 事件变了 → `EPOLL_CTL_MOD`；没兴趣了 → `EPOLL_CTL_DEL` 并转 `kDeleted` |
| `2` | `kDeleted` | 曾加过，因"零兴趣"被摘掉，**但还在 `channels_` 里** | 再想用 → `EPOLL_CTL_ADD` |

对着 `EPollPoller::updateChannel()` 的 if/else 看一遍，就明白状态机是怎么转的。

### 5.3 分层：Channel 不知道 epoll 的存在
`Channel` 只说"我变了/我下线了"（`update()`/`remove()`），
由 `EventLoop` 转给 `Poller` 接口，具体是 epoll 还是 poll，Channel 完全不关心。
所以你能在不改 Channel 一行代码的情况下换掉整个 IO 复用实现。

### 5.4 回调 + 生命周期保护（`tie` / `weak_ptr`）
`Channel` 里存的是 `std::function` 回调，回调里往往持有 `this`（比如 `TcpConnection`）。
危险场景：连接已经析构了，但 epoll 里还留着它的事件 → 回调到已销毁对象 → 段错误。
解决办法就是 `Channel::tie(shared_ptr)` + `handleEvent()` 里的 `weak_ptr::lock()`：
**提升成功才处理事件**。这个手法在 `TcpConnection` 里会大量出现。

---

## 6. 当前进度与下一步

### 已经能独立编译运行的
- `Logger` / `Timestamp` / `InAddress`：完整
- `Channel` / `Poller` / `EPollPoller` / `DefaultPoller`：编译通过

### 还没完成（按顺序做）
1. **`DefaultPoller.cpp`：把两个 `return nullptr` 改成 `return new EPollPoller(loop);`**
   （顺手 `#include "EPollPoller.h"`）—— 一行的事，但不改的话 EventLoop 拿到空指针。
2. **`EventLoop`**：按 [src/EventLoop.cpp](src/EventLoop.cpp) 里写的 5 步补成员和实现。
   写完的验证：`EventLoop loop; loop.loop();` 能安静地阻塞在 `epoll_wait` 上。
3. **`Buffer`**：收发缓冲区。
4. **`Acceptor`**：持有 listenfd 的 Channel，`accept` 新连接后回调出去。
   验证方式：`printf 'hi' | nc 127.0.0.1 6000`，看是否能打印出"新连接"。
5. **`TcpConnection`**：一个连接 = Channel + Buffer + 状态机 + `send()`。
6. **`TcpServer`**：把上面这些组装起来，对外只留两个回调。

### 编译与运行
```bash
cd /home/liangkailin/myproject
cmake -B build -S . && cmake --build build -j8   # 编译（新增/删除文件后要重新 configure）
./build/main 8888                                 # 跑真 muduo 的 echo 示例
printf 'hello\n' | timeout 2 nc 127.0.0.1 8888    # 测试
```

---

## 7. 复习自测（合上代码再答）

1. `Channel::events_` 和 `revents_` 分别由谁写入、谁读取？
2. `Channel::index_` 的三个取值是什么？分别在什么情况下出现？
3. `EPollPoller::update()` 里 `event.data.ptr = channel` 为什么不能用 `data.fd`？
4. `epoll_wait` 返回后，为什么要把 `events_` 容量翻倍？
5. 为什么 `Poller::poll` 的第二个参数是 `ChannelList*`（出参），而不是返回值？
6. `Poller` 的析构函数为什么必须写成 `virtual ~Poller() = default;`？
7. `Channel::handleEvent()` 里 `tie_`/`tied_` 是干什么的？不用它会怎样？
8. `Channel::update()` 的内部做了什么？（说出完整调用链，到 `epoll_ctl` 为止）
9. `updateChannel()` 里为什么 `isNoneEvent()` 为真时是 `EPOLL_CTL_DEL` 而不是 `ADD`？
10. `EventLoop::loop()` 里 `activeChannels_.clear()` 为什么放在循环内、而不是每轮重新构造？
11. `setThreadNum(4)` 之后，一个新连接上的读写由哪个线程的 epoll 负责？
12. `Logger` 为什么用宏实现？`LOG_DEBUG` 在没定义 `MUDEBUG` 时会发生什么？
13. `InAddress::toIpPort()` 里 `ntohs` 如果不调用，端口会打印成什么样？
14. `Timestamp::now()` 在本项目里用的是 `time(NULL)`，它和原版 muduo 的差别是什么？会带来什么后果？
15. `DefaultPoller` 这样的"工厂函数"存在有什么价值？如果直接在 `EventLoop` 里 `new EPollPoller` 会差在哪？

> 答不上来的题，就回到对应文件里看注释——每个问题的答案都写在代码注释里了。
