# 预期行为

## 通过条件

消费者依次得到 0 到 9999。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节有一个生产者线程，消费者是主线程。容量为 1024。索引原子不是始终无锁时，结果为不支持。
