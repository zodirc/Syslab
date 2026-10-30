# 预期行为

## 通过条件

/proc/self/exe 的前四个字节是 0x7f、E、L、F，EI_CLASS 为 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

section、segment 和重定位的具体布局由运行器的 ELF 观察记录描述。本程序不把某一节名写成通过条件。字节序若不是当前实验假设，EI_DATA 只作为度量输出。
