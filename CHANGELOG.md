# 变更记录

## 0.1.0

- 提供实验注册、manifest 校验、构建、子进程运行、观察记录和 17 节报告。
- 实现 arena、pool、free-list、buddy、size-class 和 thread-cache 分配器，以及匿名映射辅助。
- 实现 TAS、ticket、MCS、futex mutex、读写锁、递归互斥量、条件变量、信号量、屏障和 once。
- 实现 hazard pointer、epoch、SPSC、MPMC 和 Treiber stack。
- 实现 ucontext fiber、C++20 协程任务和工作窃取 `parallel_for`。
- 注册 CPU、内存、操作系统、同步、原子、缓存和运行时实验。
- 在缺少 `<coroutine>` 的 libstdc++ 8 上使用协程 polyfill。GCC 8 不能构建本版本。
