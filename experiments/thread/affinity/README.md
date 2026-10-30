# OS-006 cpu-affinity

## 实验目的

把进程亲和性设为 CPU 0，再读回掩码。

## 程序实际检查的内容

sched_setaffinity 成功，随后的掩码包含 CPU 0。设置失败时结果为 unsupported。

## 不由本实验断言的内容

读回掩码不证明下一条指令就在 CPU 0 上退休。本实验不调用 sched_getcpu 作为通过条件。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
