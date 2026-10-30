# 模块边界

| 目录 | 职责 |
|---|---|
| `core` | 错误、路径、时间、SHA-256、JSON 和受限制的 YAML |
| `experiment` | manifest、生命周期、注册表和一次会话 |
| `build` | 调用编译器并记录 `build.json` |
| `runner` | `fork` 加 `execve`/`execvp`，带超时和输出上限 |
| `observation` | ELF64 读取和 Markdown 报告 |
| `platform` | 环境快照、工具能力和 CPU 架构名称 |
| `memory` | 六种教学分配器和匿名映射 |
| `synchronization` | 用户态锁、futex 等待和 once |
| `atomic` | 回收算法和队列 |
| `concurrency` | fiber 调度和工作窃取 |
| `runtime` | 协程任务、所有权指针和布局打印 |

实验代码位于 `experiments/`，不反向修改运行器状态机。观察工具失败时写入不完整的观察记录，不改验证规则去迎合缺失的工具。

`platform` 只报告本机事实和能力状态。它不把 x86-64 的允许重排改写成 AArch64 的结果，也不把一次耗时解释成微架构定理。
