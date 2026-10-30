# 预期行为

## 通过条件

能读到至少两个 nodeN/cpulist 时通过。否则输出 status=unsupported。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

通过只说明拓扑目录可见。本程序不绑定内存，因此不比较节点间访问延迟。
