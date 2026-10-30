# 预期行为

## 通过条件

屏障返回后 arrived 为 4。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

到达计数本身用原子发布。屏障等待的是代数，不是忙等。
