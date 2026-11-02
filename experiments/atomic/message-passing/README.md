# AT-003 release-acquire

## 实验目的

200 次试验中，acquire 看到旗标为 1 之后，载荷都是 42。

## 程序实际检查的内容

读者看到 flag 为 1 之后，payload 必定为 42。重复 200 次。

## 不由本实验断言的内容

旗标的存储使用 memory_order_release，旗标的加载使用 memory_order_acquire。载荷的存储和加载使用 memory_order_relaxed。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
