# 资料与实现对应关系

下列资料用来核对概念边界。实验结论以当次 `report.md` 和环境快照为准，不把某本书的硬件例子写成所有 CPU 的结果。

| 主题 | 依据 | 代码或实验 |
|---|---|---|
| C++ 内存模型 | ISO C++ 标准对 happens-before、memory_order 和 data race 的定义 | AT-001 至 AT-006 |
| System V AMD64 ABI | 整数参数与返回寄存器 | CPU-004 的 x86-64 汇编 |
| AArch64 PCS | 整数参数与返回寄存器 | CPU-004 的 AArch64 汇编 |
| ELF | gABI 的 ELF 标识和程序头 | CPU-006、CPU-007、`read_elf` |
| Linux 用户态 | `fork`、`mmap`、`mprotect`、`futex`、`sched_setaffinity` 的 man 页面 | OS 实验和 `runner` |
| 缓存与分支预测 | 微架构行为，不是语言规则 | CPU-010、CACHE-001 至 CACHE-003，只记录样本 |

本仓库没有把这些文档的原文复制进树内。
