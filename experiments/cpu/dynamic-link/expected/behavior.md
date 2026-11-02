# 预期行为

## 通过条件

小端 ELF64 的程序头里存在类型为 3 的项。文件格式不符或没有这一项时，结果为不支持。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

小端 ELF64 的程序头里存在类型为 3 的项。文件格式不符或没有这一项时，结果为不支持。
