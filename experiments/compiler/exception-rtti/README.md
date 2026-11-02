# RT-002 exception-and-typeid

## 实验目的

throw Derived() 之后，catch (const Base&) 接到对象，typeid(object) 与 typeid(Derived) 相等。

## 程序实际检查的内容

throw Derived() 之后，catch (const Base&) 接到对象。typeid(object) 与 typeid(Derived) 相等。

## 不由本实验断言的内容

本节的通过条件是 typeid(object) 与 typeid(Derived) 相等。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
