# 预期行为

## 通过条件

竞争线程启动后，slow_path_count 在解锁前大于 0。八个线程的初始化函数只进入一次。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

慢路径计数不是内核等待的证明。OnceFlag 的函数若抛出异常，状态回到未完成，本实验的函数不抛出。
