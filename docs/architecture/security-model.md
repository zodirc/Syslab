# 安全模型

| 文本 | 含义 | 日常主机 |
|---|---|---|
| `readonly` | 只读检查 | 可以运行 |
| `normal` | 普通用户态程序 | 可以运行，带超时 |
| `resource_intensive` | 占用较多 CPU 或内存的用户态程序 | 可以运行，仍受超时和输出上限约束 |
| `kernel_privileged` | 内核跟踪、模块或特权接口 | 拒绝，结果为 UNSUPPORTED |
| `bare_metal` | 可能使系统不可恢复的实验 | 拒绝，结果为 UNSUPPORTED |

运行器使用参数数组执行子进程，不经过 shell。实验二进制用 `execve` 和一份环境白名单；查找 `objdump` 这类工具时才 `execvp` 并继承环境。子进程在 `fork` 之后只使用异步信号安全的调用。

超时先发送 SIGTERM，等待约 200 毫秒后再 SIGKILL。输出超过上限时终止并标记 `output_limit`。`RLIMIT_AS` 默认关闭，因为虚拟地址空间上限容易误伤正常映射；manifest 可以单独设置 `memory_bytes`。CPU 时间默认由超时换算，至少 1 秒。

这些限制不能把 R3/R4 变成可执行实验。
