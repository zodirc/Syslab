# OS-006 cpu-affinity

## 实验目的

sched_setaffinity 把调用线程的掩码设为只含 CPU 0。sched_getaffinity 读回的掩码包含 CPU 0。

## 程序实际检查的内容

sched_setaffinity 返回 0，随后 sched_getaffinity 读回的掩码包含 CPU 0。sched_setaffinity 返回非 0 时，结果为不支持。

## 不由本实验断言的内容

本节核对读回的掩码。sched_setaffinity 失败时，结果为不支持。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
