# 预期行为

## 通过条件

读者看到 flag 为 1 之后，payload 必定为 42。重复 200 次。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

这是语言内存模型的要求，不依赖 x86 TSO。relaxed 标志不能替代这里的 release/acquire。
