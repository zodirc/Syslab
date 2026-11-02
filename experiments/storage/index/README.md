# STO-003 key-index

## 实验目的

键 beta 读到 BETA。键 delta 没有索引项。

## 程序实际检查的内容

索引有 4 个定长项。`alpha`、`beta`、`gamma` 占用前三项，载荷分别是 `ALPHA`、`BETA`、`GAMMA`。查找 `beta` 得到长度 4 的 `BETA`。查找 `delta` 不返回载荷。

## 不由本实验断言的内容

键 beta 读到 BETA。键 delta 没有索引项。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
