# 预期行为

## 通过条件

相反顺序的两次 `pthread_mutex_timedlock` 都返回 `ETIMEDOUT`，更新计数为 0。相同顺序完成后更新计数为 2。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

## 边界

本实验不测量等待时间分布，也不从这次超时推断硬件内存序。
