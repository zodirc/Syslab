# 预期行为

## 通过条件

两个 fiber 依次调用 yield。run 返回后，日志为 A1B1A2B2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

两个 fiber 依次调用 yield。run 返回后，日志为 A1B1A2B2。
