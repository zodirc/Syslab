# 预期行为

## 通过条件

四个线程反复获取两个许可，观测到的最大并发不超过 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

最大并发小于 2 仍可通过，因为调度可能没有让两个持有者重叠。超过 2 才是失败。
