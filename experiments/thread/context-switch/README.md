# OS-007 voluntary-switch-metric

## 实验目的

在一批 sched_yield 前后读取自愿上下文切换计数，不把增量当作必须为正。

## 程序实际检查的内容

两次 getrusage 都成功。nvcsw 差值写入度量。

## 不由本实验断言的内容

增量为 0 仍然通过。该计数不是切换次数的语言定义，也不能代替 perf。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
