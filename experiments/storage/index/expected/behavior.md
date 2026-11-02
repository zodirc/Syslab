# 预期行为

## 通过条件

键 `beta` 的载荷是 `BETA`。键 `delta` 没有命中的索引项。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

## 边界

本实验不检查排序、范围扫描或并发更新。
