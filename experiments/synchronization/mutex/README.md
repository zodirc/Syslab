# SYNC-004 futex-mutex

## 实验目的

用四线程递增检查这把锁的互斥。

## 程序实际检查的内容

四个线程各递增 2000 次，最终计数为 8000。

## 不由本实验断言的内容

futex mutex exclusion; slow-path waits are counted separately by SYNC-010。销毁锁时不能仍有线程停在锁内。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
