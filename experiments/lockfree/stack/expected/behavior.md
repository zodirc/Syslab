# 预期行为

## 通过条件

弹出值的集合大小为 800。指针原子若不是始终无锁，结果为 unsupported。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节核对弹出集合的大小。栈顶指针的原子不是始终无锁时，结果为不支持。
