# SEC-001 owner-read-bit

## 实验目的

模式 000 时 open 返回 EACCES。改为 0644 后读到 SAFE。

## 程序实际检查的内容

`secret.bin` 写入 `SAFE`。`chmod 000` 后 `open(O_RDONLY)` 失败且 `errno` 为 `EACCES`。`chmod 0644` 后读到 `SAFE`。

## 不由本实验断言的内容

模式 000 时 open 返回 EACCES。改为 0644 后读到 SAFE。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
