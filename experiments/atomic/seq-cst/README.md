# AT-005 seq-cst-iriw

## 实验目的

200 次 memory_order_seq_cst 试验中没有出现 r1=1、r2=0、r3=1、r4=0。

## 程序实际检查的内容

不出现 r1==1、r2==0、r3==1、r4==0。

## 不由本实验断言的内容

本节的加载和存储使用 memory_order_seq_cst。通过条件覆盖这 200 次试验。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
