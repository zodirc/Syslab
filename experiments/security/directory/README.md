# SEC-003 directory-execute

## 实验目的

确认目录没有执行位时，即使里面的文件可读，`open` 也返回 `EACCES`。恢复执行位后读到 `DATA`。

## 程序实际检查的内容

`box/item` 写入 `DATA`。目录模式 `0644` 时 `open` 失败且 `errno` 为 `EACCES`。目录模式 `0755` 时读到 `DATA`。

## 不由本实验断言的内容

本实验不检查其他用户，也不检查符号链接。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
