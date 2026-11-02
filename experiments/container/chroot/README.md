# CTR-003 chroot-open

## 实验目的

chroot 之后 /data/token 读到 inside。/outside 打不开。

## 程序实际检查的内容

在私有挂载命名空间中建立 `jail/data/token`，内容为 `inside`，并在目录外建立 `outside`。`chroot` 到 `jail` 之后，`/data/token` 读到 `inside`，`/outside` 打不开。用户命名空间、挂载命名空间或 `chroot` 不能完成时，结果为不支持。

## 不由本实验断言的内容

chroot 之后 /data/token 读到 inside。/outside 打不开。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
