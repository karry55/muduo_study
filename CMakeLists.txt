cmake_minimum_required(VERSION 3.16)
project(myproject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 生成 compile_commands.json，给 IntelliSense / clangd 用

# ── 默认构建类型 ─────────────────────────────────────────────────
# 不写这一段时 CMAKE_BUILD_TYPE 是空的，而 CMake Tools 插件按 "Debug" 找配置，
# 于是 cpptools 会报「生成的生成配置不包含活动生成配置」。
# 显式给个默认值，两边就一致了。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ══════════════════════════════════════════════════════════════════
#  muduo 装在【项目内】，不需要 sudo、不污染 /usr/include：
#     third_party/muduo/include   ← 头文件
#     third_party/muduo/lib       ← 库（全是静态 .a）
# ══════════════════════════════════════════════════════════════════
set(MUDUO_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/muduo")

# ── 把「怎么连 muduo」封装成一个函数，避免每个目标重复四行配置 ──────
# 用法：  add_muduo_exe(你的目标名)
function(add_muduo_exe target)
  # 等价于 g++ -I third_party/muduo/include
  target_include_directories(${target} PRIVATE "${MUDUO_PREFIX}/include")
  # 等价于 g++ -L third_party/muduo/lib
  target_link_directories(${target} PRIVATE "${MUDUO_PREFIX}/lib")
  # 等价于 -lmuduo_net -lmuduo_base -lpthread
  target_link_libraries(${target} PRIVATE muduo_net muduo_base pthread)
  target_compile_options(${target} PRIVATE -Wall -Wextra)
  # 静态库其实用不到 rpath，留着是为了万一将来换成 .so 也不用改
  set_target_properties(${target} PROPERTIES
    BUILD_RPATH "${MUDUO_PREFIX}/lib"
    INSTALL_RPATH "${MUDUO_PREFIX}/lib")
endfunction()

# ══════════════════════════════════════════════════════════════════
#  核心规则：src/ 下每个 .cpp 各自编成一个【同名】可执行文件
#
#     src/main.cpp         →  build/main
#     src/chat_server.cpp  →  build/chat_server
#
#  注意：只有 src/ 下的 .cpp 会被编译！
#  放在 .vscode/ 等其他目录的 .cpp【不会】被编译（曾因此踩坑）。
#
#  新增源文件后必须重新配置一次：
#     cmake -B build -S . && cmake --build build
#
#  一个程序需要【多个】.cpp 时，改为显式写法：
#     add_executable(my_server src/my_server.cpp src/codec.cpp)
#     add_muduo_exe(my_server)
# ══════════════════════════════════════════════════════════════════
file(GLOB MUDUO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(NOT MUDUO_SOURCES)
  message(WARNING "src/ 下没有找到任何 .cpp 文件，不会生成可执行文件")
endif()

foreach(src ${MUDUO_SOURCES})
  get_filename_component(exe_name "${src}" NAME_WE)
  add_executable(${exe_name} "${src}")
  add_muduo_exe(${exe_name})
  message(STATUS "  将生成可执行文件: build/${exe_name}   <-  ${src}")
endforeach()
