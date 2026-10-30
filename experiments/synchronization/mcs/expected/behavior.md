# 预期行为

## 通过条件

四个线程各持锁递增 1000 次，计数为 4000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

节点必须在 unlock 返回前保持有效。McsGuard 把节点放在守卫内部。
