# 预期行为

## 通过条件

getpid() 大于 0。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

跟踪里出现哪些 libc 包装和额外调用，取决于 libcs 和加载器。名单缺失时观察记为 unavailable，实验仍以返回值为准。
