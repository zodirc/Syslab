# 预期行为

## 通过条件

两边都持有第一把锁时，对另一把锁的 trylock 返回 EBUSY，更新数是 0。相同顺序完成后更新数是 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

两边都持有第一把锁时，对另一把锁的 trylock 返回 EBUSY，更新数是 0。相同顺序完成后更新数是 2。
