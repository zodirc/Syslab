# SEC-002 symlink-follow

## 实验目的

确认 `open` 跟随符号链接读到目标文件的字节，`readlink` 返回链接中保存的路径。

## 程序实际检查的内容

`target.bin` 的内容是 `DATA`。`alias` 指向 `target.bin`。`open("alias")` 读到 `DATA`。`readlink` 返回 `target.bin`。符号链接不能建立时，结果为不支持。

## 不由本实验断言的内容

本实验不检查多层链接，也不把链接文本当成文件内容。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
