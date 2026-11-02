# 预期行为

## 通过条件

槽仍公布该地址时，删除计数为 127。clear(0) 之后，删除计数为 383。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

扫描在退役列表达到 HazardDomain::kSlots * HazardDomain::kMaxThreads 时进行。本节这个积是 128。
