# 预期行为

## 通过条件

后到的读者在已排队的写者之后进入。两个写者的计数为 200，写锁没有重叠。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

这是这个实现的写优先。POSIX 读写锁还有其他合法策略。锁不可重入。
