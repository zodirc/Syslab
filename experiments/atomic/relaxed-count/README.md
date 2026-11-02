# AT-002 relaxed-fetch-add

## 实验目的

四个线程各执行 10000 次 memory_order_relaxed 的 fetch_add，计数为 40000。

## 程序实际检查的内容

40000 次 relaxed fetch_add 之后计数为 40000。

## 不由本实验断言的内容

本节只对计数调用 fetch_add，不另写一份数据，也不读取那份数据。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
