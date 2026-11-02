# CO-001 fiber-yield

## 实验目的

两个 fiber 依次调用 yield。run 返回后，日志为 A1B1A2B2。

## 程序实际检查的内容

日志为 A1B1A2B2。

## 不由本实验断言的内容

两个 fiber 依次调用 yield。run 返回后，日志为 A1B1A2B2。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
