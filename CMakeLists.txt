cmake_minimum_required(VERSION 3.16)
project(myproject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 生成 compile_commands.json，给 IntelliSense / clangd 用

# ── 默认构建类型 ─────────────────────────────────────────────────
# 不写这一段时 CMAKE_BUILD_TYPE 是空的，而 CMake Tools 插件按 "Debug" 找配置，
# cpptools 就会报「生成的生成配置不包含活动构建配置」。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ══════════════════════════════════════════════════════════════════
#  两个外部依赖位置
#     third_party/muduo  ← 项目内安装的 muduo（头文件 + 静态库）
#     include/           ← 你自己的头文件（Logger.h / noncopyable.h ...）
# ══════════════════════════════════════════════════════════════════
set(MUDUO_PREFIX  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/muduo")
set(MY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ── 把「怎么连 muduo + 自己的 include 目录」封装成一个函数 ──────────
# 用法：  add_muduo_exe(你的目标名)
function(add_muduo_exe target)
  # 等价于 g++ -Iinclude -Ithird_party/muduo/include
  target_include_directories(${target} PRIVATE
    "${MY_INCLUDE_DIR}"
    "${MUDUO_PREFIX}/include")
  # 等价于 g++ -Lthird_party/muduo/lib
  target_link_directories(${target} PRIVATE "${MUDUO_PREFIX}/lib")
  # 等价于 -lmuduo_net -lmuduo_base -lpthread
  target_link_libraries(${target} PRIVATE muduo_net muduo_base pthread)
  target_compile_options(${target} PRIVATE -Wall -Wextra)
  set_target_properties(${target} PROPERTIES
    BUILD_RPATH "${MUDUO_PREFIX}/lib"
    INSTALL_RPATH "${MUDUO_PREFIX}/lib")
endfunction()

# ══════════════════════════════════════════════════════════════════
#  源文件自动分类（都放在 src/ 下即可，不用改这个文件）
#
#    src/xxx.cpp 里【有】 int main(...)  →  各自编成一个同名可执行文件
#         src/chat_server.cpp  →  build/chat_server
#         src/main.cpp         →  build/main
#
#    src/xxx.cpp 里【没有】 main        →  合入静态库 libcommon.a
#         src/Logger.cpp       →  build/libcommon.a
#         所有可执行文件会自动链接 libcommon.a，所以能直接用 LOG_INFO 等
#
#  新增/删除文件后重新配置一次：
#     cmake -B build -S . && cmake --build build
#
#  注意：只有 src/ 下的 .cpp 会被扫到（放在 .vscode/ 之类的地方不会被编译）
# ══════════════════════════════════════════════════════════════════
file(GLOB MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(MY_EXE_SOURCES "")
set(MY_LIB_SOURCES "")
foreach(src ${MY_SOURCES})
  # 用正则粗判有没有 main 函数（int main( / int  main ( ）
  file(STRINGS "${src}" HAS_MAIN REGEX "int[ \t]+main[ \t]*\\(")
  if(HAS_MAIN)
    list(APPEND MY_EXE_SOURCES "${src}")
  else()
    list(APPEND MY_LIB_SOURCES "${src}")
  endif()
endforeach()

# ── 公共静态库（无 main 的源文件）────────────────────────────────
if(MY_LIB_SOURCES)
  add_library(common STATIC ${MY_LIB_SOURCES})
  # PUBLIC：让链接 common 的可执行文件也自动获得这些路径与库
  target_include_directories(common PUBLIC "${MY_INCLUDE_DIR}" "${MUDUO_PREFIX}/include")
  target_link_directories(common PUBLIC "${MUDUO_PREFIX}/lib")
  target_link_libraries(common PUBLIC muduo_net muduo_base pthread)
  target_compile_options(common PRIVATE -Wall -Wextra)
  message(STATUS "  [无 main]  合入静态库 build/libcommon.a :")
  foreach(s ${MY_LIB_SOURCES})
    message(STATUS "        ${s}")
  endforeach()
endif()

# ── 可执行文件（有 main 的源文件）────────────────────────────────
foreach(src ${MY_EXE_SOURCES})
  get_filename_component(exe_name "${src}" NAME_WE)
  add_executable(${exe_name} "${src}")
  add_muduo_exe(${exe_name})
  if(MY_LIB_SOURCES)
    target_link_libraries(${exe_name} PRIVATE common)
  endif()
  message(STATUS "  [有 main]  生成可执行文件 build/${exe_name}  <-  ${src}")
endforeach()

if(NOT MY_SOURCES)
  message(WARNING "src/ 下没有找到任何 .cpp 文件")
endif()
