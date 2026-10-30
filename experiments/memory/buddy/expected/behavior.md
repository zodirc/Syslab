# 预期行为

## 通过条件

8 个 24 字节块各自保持写入的填充值。全部释放后，128 字节请求成功。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

伙伴算法的阶和最小块是实现参数。本实验不把某一阶的空闲链表长度写成可移植结论。
