# 预期行为

## 通过条件

两种循环的校验和都不为 0。耗时以微秒度量写入 SYSLAB_METRIC。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

分支预测命中率、性能计数器事件名和谁更快都属于微架构观察。本主机若没有 perf，观察记录会标明未采样。
