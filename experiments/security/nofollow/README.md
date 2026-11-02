# SEC-004 no-follow

## 实验目的

确认 `O_NOFOLLOW` 遇到作为最后一段的符号链接时返回 `ELOOP`。不带该标志时读到目标中的 `DATA`。

## 程序实际检查的内容

`open("alias", O_RDONLY | O_NOFOLLOW)` 失败且 `errno` 为 `ELOOP`。`open("alias", O_RDONLY)` 读到 `DATA`。符号链接不能建立时，结果为不支持。

## 不由本实验断言的内容

本实验不检查路径中间某一段上的符号链接。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
