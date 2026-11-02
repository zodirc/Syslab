# 预期行为

## 通过条件

源缓冲填满 0x3c。memcpy 复制 8 MiB 之后，目的缓冲与源缓冲逐字节相同。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

源缓冲填满 0x3c。memcpy 复制 8 MiB 之后，目的缓冲与源缓冲逐字节相同。
