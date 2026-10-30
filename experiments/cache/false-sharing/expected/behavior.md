# 预期行为

## 通过条件

两种布局的计数和都是 200000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

相邻数组的两个元素是否落在同一缓存行取决于 ABI 布局。耗时差异不是通过条件。
