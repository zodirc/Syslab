# 预期行为

## 通过条件

不出现 r1==1、r2==0、r3==1、r4==0。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节的加载和存储使用 memory_order_seq_cst。通过条件覆盖这 200 次试验。
