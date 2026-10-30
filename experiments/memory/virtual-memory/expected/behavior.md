# 预期行为

## 通过条件

映射基址非空，映射长度至少 8192，写入的最后一个字节读回为 0x5a。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

缺页是否发生、驻留页数量是否增加，需要 mincore 或性能计数器。本实验不把缺页次数当作通过条件。
