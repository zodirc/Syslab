# 预期行为

## 通过条件

第一次 CAS 把 tag 从 0 改为 1。随后用 tag 仍为 0 的期望值做 CAS 必须失败，指针保持为 1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

这不是无锁栈里真实发生的 ABA。它只演示版本号使复用后的旧期望值失效。Slot 必须是可平凡复制的，本结构满足这一点。
