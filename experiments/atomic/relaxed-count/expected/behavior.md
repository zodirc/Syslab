# 预期行为

## 通过条件

40000 次 relaxed fetch_add 之后计数为 40000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不把 relaxed 操作当成发布了其他内存的同步边。
