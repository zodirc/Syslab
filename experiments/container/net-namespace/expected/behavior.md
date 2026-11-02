# 预期行为

## 通过条件

新网络命名空间中 `lo` 的接口索引大于 0，进入之前选中的那块网卡没有索引。原命名空间仍能索引那块网卡。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

没有合适的接口，或用户命名空间、网络命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验通过 `ioctl` 查询接口索引，不读取 `/sys/class/net`，也不配置地址或路由。
