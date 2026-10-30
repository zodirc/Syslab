# OS-010 numa-nodes

## 实验目的

统计 sysfs 中可见的 NUMA 节点。少于两个节点时明确返回 unsupported。

## 程序实际检查的内容

能读到至少两个 nodeN/cpulist 时通过。否则输出 status=unsupported。

## 不由本实验断言的内容

通过只说明拓扑目录可见。本程序不绑定内存，因此不比较节点间访问延迟。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
