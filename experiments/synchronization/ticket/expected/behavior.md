# 预期行为

## 通过条件

先取号的线程先进入。四个线程各递增 2000 次，计数为 8000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

锁仍被持有或仍有线程等待时不得销毁。不可重入。
