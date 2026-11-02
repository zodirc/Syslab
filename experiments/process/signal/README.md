# OS-009 signal-delivery

## 实验目的

sigaction 安装 SIGUSR1 的处理函数后，raise 使 delivered 变为 1。

## 程序实际检查的内容

sig_atomic_t 计数变为 1。

## 不由本实验断言的内容

处理函数只写 sig_atomic_t。系统调用跟踪是观察，不要求固定的 rt_sigaction 参数。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
