# RT-002 exception-and-typeid

## 实验目的

通过基类处理函数捕获派生异常，并用 typeid 核对动态类型。

## 程序实际检查的内容

catch (const Base&) 捕获 Derived，typeid 与 Derived 相等。

## 不由本实验断言的内容

异常表和 typeinfo 的二进制布局属于 ABI。本实验不比对具体符号名。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
