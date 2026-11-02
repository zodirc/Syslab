# 预期行为

## 通过条件

超时计数为 1，回答计数为 1，发送 `FAST` 的连接读到 `OK`。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

## 边界

这次关闭使用程序内 400 毫秒的 `poll` 期限。它不使用运行器在 `timeout_ms` 到期时写入的 `timed_out`。
