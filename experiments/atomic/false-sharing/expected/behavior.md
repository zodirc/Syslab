# 预期行为

## 通过条件

两种布局的两次递增之和都是 400000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

谁更快取决于微架构和干扰。通过条件不比较两个耗时。
