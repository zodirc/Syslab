# RT-001 object-layout

## 实验目的

Sample 是标准布局。offsetof(Sample, first) 为 0。对象起始地址读出的 4 字节等于 first。

## 程序实际检查的内容

offsetof(Sample, first) 为 0。first 写成 0xA1B2C3D4 后，对象起始地址读出的 4 字节等于 first。

## 不由本实验断言的内容

本节核对 Sample 的首字段。offsetof(Sample, first) 为 0，对象起始地址读出的 4 字节等于 0xA1B2C3D4。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
