# CPU-006 elf-header

## 实验目的

读取当前可执行文件的 ELF 标识，确认它是 ELF64。

## 程序实际检查的内容

/proc/self/exe 的前四个字节是 0x7f、E、L、F，EI_CLASS 为 2。

## 不由本实验断言的内容

section、segment 和重定位的具体布局由运行器的 ELF 观察记录描述。本程序不把某一节名写成通过条件。字节序若不是当前实验假设，EI_DATA 只作为度量输出。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
