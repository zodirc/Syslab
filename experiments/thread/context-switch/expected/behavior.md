# 预期行为

## 通过条件

两次 getrusage 都成功。nvcsw 差值写入度量。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

增量为 0 仍然通过。该计数不是切换次数的语言定义，也不能代替 perf。
