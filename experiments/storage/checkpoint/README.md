# STO-007 checkpoint-replay

## 实验目的

检查点镜像 KEEP 保留。lsn 2 写入后，页是 KEEPTAIL，applied_lsn 是 2。

## 程序实际检查的内容

磁盘页最初是 8 个 `.`，`applied_lsn` 为 0。检查点镜像是 `KEEP....`，`lsn` 为 1。日志中 `lsn` 1 在偏移 0 写入 `LOST`，`lsn` 2 在偏移 4 写入 `TAIL`。恢复后的页是 `KEEPTAIL`，`applied_lsn` 为 2。`fsync` 失败时结果为不支持。

## 不由本实验断言的内容

检查点镜像 KEEP 保留。lsn 2 写入后，页是 KEEPTAIL，applied_lsn 是 2。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
