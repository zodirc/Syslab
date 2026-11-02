# CTR-002 mount-namespace

## 实验目的

新挂载命名空间里的 vis/marker 读回 in。进入命名空间之前的进程看不到该文件。

## 程序实际检查的内容

进入新挂载命名空间之前先 `fork`。当前进程建立用户命名空间并映射为 0，再 `unshare(CLONE_NEWNS)`，对 `/` 执行 `MS_REC|MS_PRIVATE`，然后在 `vis` 上挂载 tmpfs 并写入 `vis/marker`。本进程读回 `in`。命名空间外的进程对 `vis/marker` 的 `access` 失败。用户命名空间或挂载命名空间不能建立时，结果为不支持。

## 不由本实验断言的内容

新挂载命名空间里的 vis/marker 读回 in。进入命名空间之前的进程看不到该文件。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
