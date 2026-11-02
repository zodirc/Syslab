# 预期行为

## 通过条件

步长 1 和步长 16 的校验和分别等于按步长累加的结果。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

缓存命中和谁更快不是通过条件。CPU 型号在环境快照里，不从耗时反推。
