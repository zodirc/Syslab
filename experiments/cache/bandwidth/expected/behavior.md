# 预期行为

## 通过条件

目标缓冲区与源缓冲区的 8 MiB 内容一致。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

单次 memcpy 耗时不是带宽中位数，也没有固定频率或 NUMA 绑定。报告不应把它写成硬件上限。
