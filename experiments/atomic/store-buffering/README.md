# AT-004 seq-cst-store-buffering

## 实验目的

300 次 memory_order_seq_cst 试验中，两个加载没有同时为 0。

## 程序实际检查的内容

300 次试验里不出现 r1 与 r2 同时为 0。

## 不由本实验断言的内容

本节的存储和加载使用 memory_order_seq_cst。通过条件覆盖这 300 次试验。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
