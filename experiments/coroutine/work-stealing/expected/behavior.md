# 预期行为

## 通过条件

总和等于 999 * 1000 / 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

单次切片必须放进固定容量的双端队列。这个调度器是教学实现，任务体的异常会在 join 之后重新抛出。
