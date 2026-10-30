# 预期行为

## 通过条件

四个生产者各发送 500 个互异值，消费者集合大小为 2000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

队列有界。T 必须可平凡复制。进度声明依赖索引原子始终无锁。
