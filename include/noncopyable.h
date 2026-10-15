#pragma once

/*
================================================================================
【基础模块 1/5】noncopyable —— "禁止拷贝"的基类
================================================================================
它解决什么问题：
    一个对象如果内部持有"资源"（fd、堆内存、互斥量…），被拷贝就是灾难 ——
    两个对象析构时都去 close 同一个 fd，程序直接崩。
    继承 noncopyable 以后，编译器会替你把这类错误拦在编译期。

谁继承了它（本项目）：
    Channel、Poller、Logger（还有你将来的 EventLoop、TcpServer）

怎么用：
    class Channel : noncopyable { ... };
    Channel a(loop, fd);
    Channel b = a;   // 编译报错：拷贝构造被 delete 了

C++11 的关键字解释：
    = delete        把函数"删除"，比老写法（声明成 private 且不实现）报错更直观
    = default       让编译器生成默认实现（这里是空的构造/析构）
    protected 构造  外面无法直接 new noncopyable，只有派生类能调用
================================================================================
*/

// noncopyable 被继承以后,派生类对象可以正常的构造和析构,但是派生类对象
// 无法进行拷贝构造和赋值操作
class noncopyable {
public:
  // 拷贝构造、拷贝赋值都"删除"：任何拷贝尝试都会在编译期报错
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;

protected:
  // 放在 protected：本类不能被单独实例化，但派生类可以正常构造/析构自己
  noncopyable() = default;
  ~noncopyable() = default;
};
