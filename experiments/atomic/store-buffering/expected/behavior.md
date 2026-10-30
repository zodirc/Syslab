# 预期行为

## 通过条件

300 次试验里不出现 r1 与 r2 同时为 0。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

relaxed 或 acq_rel 允许两个加载都看到 0。本实验不运行那个允许的结果作为失败条件。没有观察到违规只覆盖这 300 次样本。
