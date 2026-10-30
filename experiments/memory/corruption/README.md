# MEM-010 allocator-checks

## 实验目的

检查分配器拒绝 0 字节请求，忽略空释放，并在重复释放时报告错误。

## 程序实际检查的内容

allocate({0}) 抛出 syslab::Error。deallocate(nullptr) 不抛出。对同一块的第二次 deallocate 抛出 syslab::Error。

## 不由本实验断言的内容

这些检查属于教学分配器。它们不替代 ASan，也不说明全局 new 有同样行为。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
