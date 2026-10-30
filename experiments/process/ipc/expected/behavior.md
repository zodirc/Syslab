# 预期行为

## 通过条件

管道读回 ok。子进程写入 42 后，父进程从共享映射读到 42。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不比较 pipe、socket 和共享内存的带宽，也不把一次耗时当成性能结论。
