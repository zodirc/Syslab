# 预期行为

## 通过条件

读者观察到 flag 为 1 并执行 acquire fence 之后，payload 为 42。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

编译器屏障、C++ atomic_thread_fence、ISA 屏障和缓存一致性不是同一个机制。这里检查的是 C++ fence。
