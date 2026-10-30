# 预期行为

## 通过条件

ELF64 小端文件的程序头中存在类型为 PT_INTERP（3）的段。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

没有解释器的静态链接文件得到 unsupported。PLT 槽的解析时机和 GOT 内容不作为数值断言。
