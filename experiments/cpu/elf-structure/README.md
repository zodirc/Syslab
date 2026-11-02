# CPU-006 elf-header

## 实验目的

/proc/self/exe 的前四个字节是 0x7f、E、L、F。第 5 个字节为 2。打不开文件或类别字节不是 2 时，结果为不支持。

## 程序实际检查的内容

/proc/self/exe 的前四个字节是 0x7f、E、L、F，EI_CLASS 为 2。

## 不由本实验断言的内容

/proc/self/exe 的前四个字节是 0x7f、E、L、F。第 5 个字节为 2。打不开文件或类别字节不是 2 时，结果为不支持。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
