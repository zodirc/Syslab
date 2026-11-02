# OS-005 condition-wake

## 实验目的

通知方持锁把 ready 写成 true 并调用 notify_one。等待者从 wait 返回后，observed 为 true。

## 程序实际检查的内容

等待线程从 wait 返回后把 observed 写成 true。join 之后 observed 为 true。

## 不由本实验断言的内容

本节的谓词读取 ready。通过条件是 join 之后 observed 为 true。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
