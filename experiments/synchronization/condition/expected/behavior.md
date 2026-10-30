# 预期行为

## 通过条件

等待线程在 value 变为 7 之后返回。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

允许伪唤醒。谓词在持锁时复查。本实验不证明 futex 一定进入了睡眠。
