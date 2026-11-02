# MEM-008 mprotect-fault

## 实验目的

子进程把页改为 PROT_NONE 并写入。父进程读到子进程因 SIGSEGV 终止。

## 程序实际检查的内容

子进程因 SIGSEGV 终止。父进程继续并输出通过标记。

## 不由本实验断言的内容

父进程 waitpid 读到 WTERMSIG 为 SIGSEGV。父进程不写入该页。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
