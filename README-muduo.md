# muduo 环境配置说明（myproject）

> 由 AI 助手按你的教程逐步执行后写成，记录了**教程与你机器实际不符的地方**，避免下次重踩。

## 一、最终目录布局

```
~/myproject/
├── CMakeLists.txt              ← 你的工程（已配好 muduo 的头文件/库/链接参数）
├── src/main.cpp                ← muduo echo server 最小示例
├── muduo/                      ← muduo 源码（git clone，HEAD f1fc77e）
├── build/release-cpp11/        ← 编译中间产物（cmake 的 build 目录）
├── build-muduo.log             ← 完整编译日志
└── third_party/muduo/          ← install 产物 ★ 工程引用的就是这里
    ├── include/muduo/...
    └── lib/libmuduo_{base,net,http,curl,inspect}.a   ← 注意：全是静态库 .a
```

## 一·五、实测验证结果（已跑通，非推测）

```
$ ./build/main 18899 &
$ printf 'hello-muduo\n' | nc 127.0.0.1 18899
hello-muduo                    ← 回显一致 → ECHO_OK
```

- `build-muduo.log` 里 `grep -c "error:"` = **0**
- `cmake -B build -S . && cmake --build build` 全绿，产出 `build/main`（3.7 MB）
- 链接参数 `-lmuduo_net -lmuduo_base -lpthread` 实测可用
- 因为库是**静态**的（只有 `.a`），运行时**不依赖** `LD_LIBRARY_PATH`；`CMakeLists.txt` 里那两行 rpath 对静态库是空操作，留着无害

## 二、实际执行的编译 / 安装命令

```bash
cd ~/myproject/muduo

# ① 先拆掉 GCC 15 会踩的 -Werror（原件已备份为 CMakeLists.txt.orig）
sed -i s/-Werror/-Wno-error/ CMakeLists.txt

# ② 编译（本机 32 核，用 -j24）
CMAKE_POLICY_VERSION_MINIMUM=3.5 \
BUILD_DIR=$PWD/../build \
INSTALL_DIR=$PWD/../third_party/muduo \
./build.sh -j24

# ③ 再跑一次带 install 参数（install 目标）
CMAKE_POLICY_VERSION_MINIMUM=3.5 \
BUILD_DIR=$PWD/../build \
INSTALL_DIR=$PWD/../third_party/muduo \
./build.sh install
```

## 三、你的教程里三处与实际不符（重要）

| # | 教程写的 | 实际情况 | 处理 |
|---|---|---|---|
| 1 | 直接 `./build.sh` | muduo 的 `CMakeLists.txt` 是 `cmake_minimum_required(VERSION 2.6)`，而本机是 **CMake 4.2.3**，CMake 4 已移除对 < 3.5 的兼容，会直接报错退出 | 加环境变量 `CMAKE_POLICY_VERSION_MINIMUM=3.5`，**不需要改源码** |
| 2 | 产物在 `../build/release-install-cpp11` | 当前 master 的 `build.sh` 里是 `INSTALL_DIR=${INSTALL_DIR:-../release-install-cpp11}`，**没有 build/ 那一层** | 显式传 `INSTALL_DIR`，落到项目内 `third_party/muduo` |
| 3 | `sudo mv include/muduo /usr/include/` | 需要 sudo 密码，且会污染系统目录 | 改为装在项目内，**零 sudo**；真要系统级见第五节 |
| 4 | 直接 `./build.sh` 就能编过 | **GCC 15.2 把老代码的警告升级成了致命错误**：`muduo/net/http/HttpResponse.cc:32` 触发 `-Werror=format-truncation`（库本体！），`examples/asio/chat/*.cc` 触发 5 处 `-Werror=array-bounds`。编译停在 37%，`muduo_net` 根本没生成 | 把 `CMakeLists.txt` 第 23 行的 `-Werror` 改成 `-Wno-error`（保留全部警告，只是不中断编译）。原件备份在 `muduo/CMakeLists.txt.orig` |

**另外一个不在教程里、但一定会撞上的坑**：如果源码是从 Windows 侧克隆/解压再拷进 WSL 的，`build.sh` 会报
`bash: ./build.sh: /bin/sh^M: bad interpreter` —— 因为 Windows git 的 `core.autocrlf` 把换行符换成了 CRLF。
修复（保留 `.git` 时最可靠）：

```bash
cd ~/myproject/muduo
git config core.autocrlf false && git config core.eol lf
rm -f .git/index && git reset --hard HEAD
```

## 四、编译你自己的代码

```bash
cd ~/myproject
cmake -B build -S . && cmake --build build
./build/main 8888          # 启动 echo 服务器
```

`CMakeLists.txt` 里已经写好的等价关系：

| CMake 写法 | 等价的 g++ 参数 |
|---|---|
| `target_include_directories(... third_party/muduo/include)` | `-I third_party/muduo/include` |
| `target_link_directories(... third_party/muduo/lib)` | `-L third_party/muduo/lib` |
| `target_link_libraries(... muduo_net muduo_base pthread)` | `-lmuduo_net -lmuduo_base -lpthread` |
| `BUILD_RPATH ...` | `-Wl,-rpath,<lib 目录>`（免设 `LD_LIBRARY_PATH`） |

不用 CMake、直接 g++ 时：

```bash
g++ -std=c++17 -I third_party/muduo/include your.cc \
    -L third_party/muduo/lib -lmuduo_net -lmuduo_base -lpthread \
    -Wl,-rpath,$PWD/third_party/muduo/lib
```

## 五、想让**所有**项目都能用（系统级安装，需要你自己输密码）

```bash
cd ~/myproject/third_party/muduo
sudo cp -r include/muduo /usr/local/include/
sudo cp lib/*        /usr/local/lib/
sudo ldconfig

# 验证
ls /usr/local/include/muduo/net/TcpServer.h
ls /usr/local/lib/libmuduo_net*
```

> 你的教程写的是 `/usr/include/`。用 `/usr/local/include/` 更规范（`/usr/local` 是留给本地安装的），
> gcc 两个目录都会搜，效果一样。若按教程放到 `/usr/include/`，那 `lib` 也建议配套放 `/usr/local/lib/` 并 `ldconfig`。

## 六、WSL 无法访问 GitHub（与 muduo 无关，但会挡路）

排查结果：**WSL 内完全没有外网**，直连、网关代理、配置里的代理三种全部失败。根因是 `~/.gitconfig` 里写着：

```
http.proxy=http://10.255.255.254:7890
https.proxy=http://10.255.255.254:7890
```

但这台 WSL 的默认网关是 `172.17.144.1`（`ip route` 可见），`10.255.255.254:7890` 根本连不上 —— 所有 git HTTPS 都被这个失效代理卡死。

**本次的绕法**：在 Windows 侧 `git clone`（那边能通），再把源码拷进 WSL。

**修复建议**（二选一）：

```bash
# 方案 A：你确实有代理（Windows 上跑 Clash/v2ray 并开了「允许局域网连接」）
git config --global http.proxy  http://172.17.144.1:7890
git config --global https.proxy http://172.17.144.1:7890

# 方案 B：不用代理（前提是你的网络能直连 GitHub）
git config --global --unset http.proxy
git config --global --unset https.proxy
```

> 注意：`172.17.144.1` 是**当前**这次 WSL 启动的网关地址，WSL 重启后可能变化。

## 七、遗留事项

- `.vscode/moduo_server.cpp`（0 字节，20:50 创建）是你自己建的文件，**不是我放的**。`.vscode/` 是放编辑器配置的目录，`.cpp` 放这里不会被 CMake 编译。建议移到 `src/` 并改名 `muduo_server.cpp`。

---

## 八、CMakeLists.txt 已升级（重要，替代第四节的编译说明）

新规则：**`src/` 下每个 `.cpp` 各自编成一个同名可执行文件** —— 从此"二进制名 == 源文件名"，
不会再出现"我改了代码，但跑的是旧二进制"这种混乱。

| 源文件 | 生成的可执行文件 |
|---|---|
| `src/main.cpp` | `build/main` |
| `src/chat_server.cpp` | `build/chat_server` |
| `src/xxx.cpp` | `build/xxx` |

> ⚠️ 名字变了：以前 `src/main.cpp` 生成的是 `build/myproject`，**现在叫 `build/main`**。

新增源文件后（必须重新配置一次）：

```bash
cd ~/myproject
cmake -B build -S . && cmake --build build
```

一个程序需要**多个** `.cpp` 时（例如自己的 `codec.cc`），别依赖自动扫描，显式写：

```cmake
add_executable(my_server src/my_server.cpp src/codec.cpp)
add_muduo_exe(my_server)          # 这个函数已在 CMakeLists.txt 里定义好
```

## 九、muduo 自带示例（已经编译好了，直接用）

`build/release-cpp11/bin/` 下有 **100+ 个**示例可执行文件，包括：

```
asio_chat_server                      ← ChatServer（就是你说的那个）
asio_chat_client
asio_chat_server_threaded
asio_chat_server_threaded_efficient
asio_chat_server_threaded_highperformance
simple_echo / netty_echo_server / pingpong_server / sudoku_* ...
```

**ChatServer 的端口是命令行参数，源码里没有硬编码 6000**（见 `muduo/examples/asio/chat/server.cc` 第 69-84 行：
`if (argc > 1) { uint16_t port = atoi(argv[1]); ... } else { printf("Usage: %s port\n", argv[0]); }`）。
所以要么给它参数，要么在你自己的源码里写死端口：

```bash
./build/release-cpp11/bin/asio_chat_server 6000
```

> 🕳️ 一个容易误判的坑：muduo 的日志走 `stdout`，而 **stdout 重定向到文件时是块缓冲**，
> 进程被 `kill` 时缓冲区里的日志会丢失 —— 日志文件空不代表程序没起来。
> 在 VS Code 集成终端里直接跑就能看到日志。

## 十、截至目前的真实源码状态

| 文件 | 内容 | 说明 |
|---|---|---|
| `src/main.cpp` | muduo echo server，默认端口 **8888** | 助手写的示例，可用 |
| `src/chat_server.cpp` | **你的 ChatServer，写死 `127.0.0.1:6000`**（2806 字节） | 原名 `.vscode/moduo_server.cpp`，已在 22:35 移入 `src/` 并改名；编译产出 `build/chat_server` |

**现在项目里已经有你的 6000 端口 ChatServer 了**（`src/chat_server.cpp`）。两种跑法：

1. 直接跑示例：`./build/release-cpp11/bin/asio_chat_server 6000`
2. 把自己的代码存成 `src/chat_server.cpp`（记得保存到磁盘！），再 `cmake -B build -S . && cmake --build build`，
   然后 `./build/chat_server 6000`


## 十一、2026-10-12 22:35 修正（覆盖第十节的判断）

**结论纠正：你的 ChatServer 源码一直存在，是 2806 字节，不是空文件。**

真正的问题是**它被放在 `.vscode/` 目录里**，而 CMake 只扫描 `src/` —— 所以它**从来没被编译过**。
这就是"我改了代码，跑出来却总是旧程序（8888）"的根因，跟"二进制没更新"无关，是"源码压根没进构建"。

处理：
- 移动并改名：`.vscode/moduo_server.cpp` → **`src/chat_server.cpp`**（代码一行未改）
- 编译产出：**`build/chat_server`**
- 实测（你的代码原样）：

```
$ ./build/chat_server &
$ printf 'hello-chat\n' | nc 127.0.0.1 6000
hello-chat

# 服务器端日志
INFO TcpServer::newConnection [ChatServer] - new connection [ChatServer-127.0.0.1:6000#1] from 127.0.0.1:54276
127.0.0.1:54276 - >127.0.0.1:6000state :online
recv data :hello-chat
 time: ...
127.0.0.1:54276 - >127.0.0.1:6000 state:offline
```

**附带发现的小瑕疵**：`onConnection` 里 `toIpPort() << " - >"` 与 `"state :online"` 之间少了空格，
所以日志打成 `6000state :online`。想好看就把 `"state :online"` 改成 `" state :online"`（两处）。

## 十二、CMake Tools 插件报 "No rule to make target 'myproject'" 的原因与修法

原因：CMakeLists 改版后**目标名从 `myproject` 变成了 `main` / `chat_server`**，
但 CMake Tools 插件把**旧的已选构建目标**缓存在工作区状态里，仍然去构建 `myproject`，
而新的构建系统里已经没有这个目标了 → `gmake: *** No rule to make target 'myproject'.  Stop.`

修法（命令标题已从你本机 `ms-vscode.cmake-tools-1.25.3/package.nls.json` 逐条核对，直接照着搜即可）：

1. **最直接**：`Ctrl+Shift+P` → 输入 `CMake` → 执行 **CMake: Set Build Target**
   （命令 ID `cmake.setDefaultTarget`）→ 在列表里改选 **`chat_server`** 或 **`all`**。
   这一步就把缓存的旧目标 `myproject` 替换掉了。
2. **想一步到位**：执行 **CMake: Delete Cache, Reconfigure and Build**
   （`cmake.cleanConfigureAndBuild`）—— 删缓存 + 重新配置 + 编译，全自动。
   其中 **CMake: Clean Rebuild**（`cmake.cleanRebuild`）是它的轻量版。
3. 等价操作：点 VS Code 底部状态栏 CMake 区域里**显示当前构建目标的那个按钮**，
   在弹出的列表里改选目标，效果同第 1 条。
4. 兜底：`cmake.resetState`（命令面板搜 `CMake: Reset`，用于故障排查），
   在当前状态彻底错乱、前三条都无效时用，用完需要 `Developer: Reload Window`。

**关于另一条警告**「生成的生成配置不包含活动生成配置。对 CMAKE_BUILD_TYPE 使用 "" 而不是 "Debug"」：
已通过在 `CMakeLists.txt` 里显式设默认构建类型修好：

```cmake
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
```

这样 CMakeCache 里的 CMAKE_BUILD_TYPE 与插件的活动配置（Debug）一致，IntelliSense 就能找到配置了。
