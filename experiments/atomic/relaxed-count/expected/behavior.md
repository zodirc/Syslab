# 预期行为

## 通过条件

40000 次 relaxed fetch_add 之后计数为 40000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节只对计数调用 fetch_add，不另写一份数据，也不读取那份数据。
