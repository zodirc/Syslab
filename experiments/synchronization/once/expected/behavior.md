# 预期行为

## 通过条件

八个线程的初始化函数只进入一次。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

函数抛出异常时，状态写回 0。本节的函数不抛出异常。
