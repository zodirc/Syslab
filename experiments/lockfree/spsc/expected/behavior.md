# 预期行为

## 通过条件

消费者依次得到 0 到 9999。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

第二个生产者或第二个消费者不属于这个算法。容量必须是 2 的幂。
