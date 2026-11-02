# 预期行为

## 通过条件

/proc/self/exe 的前四个字节是 0x7f、E、L、F。第 5 个字节为 2。打不开文件或类别字节不是 2 时，结果为不支持。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

/proc/self/exe 的前四个字节是 0x7f、E、L、F。第 5 个字节为 2。打不开文件或类别字节不是 2 时，结果为不支持。
