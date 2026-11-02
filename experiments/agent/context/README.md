# AG-002 context-budget

## 实验目的

预算 12 字节。hello 已丢。剩下 world、!!、xxxx，最新一条是 xxxx。

## 程序实际检查的内容

依次写入 `hello`、`world`、`!!`、`xxxx` 后，`hello` 不在，其余三条在，总字节数不超过 12，最新一条是 `xxxx`。

## 不由本实验断言的内容

预算 12 字节。hello 已丢。剩下 world、!!、xxxx，最新一条是 xxxx。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
