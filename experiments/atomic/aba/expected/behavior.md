# 预期行为

## 通过条件

字初值为 1。第一次比较交换写成 (1<<32)|1。期望值仍为 1 的比较交换失败，字保持为 (1<<32)|1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节比较的是一个 uint64_t。高 32 位是版本，低 32 位是值。
