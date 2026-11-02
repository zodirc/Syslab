# SEC-003 directory-execute

## 实验目的

目录模式 0644 时 open 返回 EACCES。改为 0755 后读到 DATA。

## 程序实际检查的内容

`box/item` 写入 `DATA`。目录模式 `0644` 时 `open` 失败且 `errno` 为 `EACCES`。目录模式 `0755` 时读到 `DATA`。

## 不由本实验断言的内容

目录模式 0644 时 open 返回 EACCES。改为 0755 后读到 DATA。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
