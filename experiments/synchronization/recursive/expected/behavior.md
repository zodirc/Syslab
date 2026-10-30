# 预期行为

## 通过条件

同一线程 lock 两次再 unlock 两次。随后四线程计数为 2000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

非拥有者不能利用递归计数进入临界区。异常路径下守卫析构会释放一次。
