# SYNC-010 once-initialization

## 实验目的

八个线程调用 call_once，初始化函数进入一次。

## 程序实际检查的内容

八个线程对同一个 OnceFlag 调用 call_once。函数把计数加 1。调用结束时计数为 1。

## 不由本实验断言的内容

函数抛出异常时，OnceFlag 把状态写回 0，其他调用者可以重试。本节的函数不抛出异常。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
