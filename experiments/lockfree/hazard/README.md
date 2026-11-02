# AT-012 hazard-pointer

## 实验目的

槽仍公布该地址时，删除计数为 127。clear(0) 之后，删除计数为 383。

## 程序实际检查的内容

槽仍公布该地址时，删除计数为 127。clear(0) 之后，删除计数为 383。

## 不由本实验断言的内容

扫描在退役列表达到 HazardDomain::kSlots * HazardDomain::kMaxThreads 时进行。本节这个积是 128。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
