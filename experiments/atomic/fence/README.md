# AT-006 release-acquire-fence

## 实验目的

用 relaxed 原子和 atomic_thread_fence 传递 payload。

## 程序实际检查的内容

读者观察到 flag 为 1 并执行 acquire fence 之后，payload 为 42。

## 不由本实验断言的内容

编译器屏障、C++ atomic_thread_fence、ISA 屏障和缓存一致性不是同一个机制。这里检查的是 C++ fence。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
