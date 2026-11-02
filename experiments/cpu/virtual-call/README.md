# CPU-005 virtual-call

## 实验目的

Base* 指向 Derived 对象时，value() 返回 7。

## 程序实际检查的内容

通过 Base* 调用 Derived::value 得到 7。

## 不由本实验断言的内容

Base* 指向 Derived 对象时，value() 返回 7。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
