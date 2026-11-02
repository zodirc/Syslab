# OS-001 getpid-syscall

## 实验目的

getpid 返回正数。跟踪中的入口以本次记录为准。

## 程序实际检查的内容

getpid() 大于 0。

## 不由本实验断言的内容

跟踪里出现哪些 libc 包装和额外调用，取决于 libcs 和加载器。名单缺失时观察记为 unavailable，实验仍以返回值为准。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
