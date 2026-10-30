# AT-003 release-acquire

## 实验目的

检查 release 存储与 acquire 加载之间的消息传递。

## 程序实际检查的内容

读者看到 flag 为 1 之后，payload 必定为 42。重复 200 次。

## 不由本实验断言的内容

这是语言内存模型的要求，不依赖 x86 TSO。relaxed 标志不能替代这里的 release/acquire。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
