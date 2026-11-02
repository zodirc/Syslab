# 预期行为

## 通过条件

offsetof(Sample, first) 为 0。first 写成 0xA1B2C3D4 后，对象起始地址读出的 4 字节等于 first。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节核对 Sample 的首字段。offsetof(Sample, first) 为 0，对象起始地址读出的 4 字节等于 0xA1B2C3D4。
