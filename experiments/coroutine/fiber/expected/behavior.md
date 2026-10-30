# 预期行为

## 通过条件

日志为 A1B1A2B2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

这是 POSIX ucontext 教学切换，不能在信号处理函数里调用，也不抢占不让出的 fiber。
