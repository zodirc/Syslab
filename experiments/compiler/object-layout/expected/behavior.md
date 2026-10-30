# 预期行为

## 通过条件

Sample::first 的偏移为 0。Base 是多态类型，id() 返回 1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

非标准布局类型的字段偏移不是语言保证。虚表位置只出现在观察输出的标注里。
