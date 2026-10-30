# 预期行为

## 通过条件

等待线程在 notify 之后把 observed 设为真。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

进入内核等待不等于发生了线程切换。本实验不命名调度器函数，也不要求 perf sched。
