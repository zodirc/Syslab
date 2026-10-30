# 预期行为

## 通过条件

不出现 r1==1、r2==0、r3==1、r4==0。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

200 次没有出现该结果，只说明这批样本没有违反约束。它不证明硬件从未允许更弱的非 seq_cst 结果。
