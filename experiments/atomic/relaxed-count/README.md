# AT-002 relaxed-fetch-add

## 实验目的

区分原子性和顺序：relaxed fetch_add 的总和仍然精确。

## 程序实际检查的内容

40000 次 relaxed fetch_add 之后计数为 40000。

## 不由本实验断言的内容

本实验不把 relaxed 操作当成发布了其他内存的同步边。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
