# 数据模型

公开结构定义在 `include/syslab/experiment/model.hpp`。

`ExperimentState` 使用设计中的 12 个状态名。`RiskLevel` 的文本是 `readonly`、`normal`、`resource_intensive`、`kernel_privileged`、`bare_metal`。

`SampledField` 表示一次采集。`status` 为 `Ok`、`Unavailable`、`Unsupported` 或 `Failed`。调用方先看状态；状态不是 `Ok` 时，`value` 不是测量结果。空字符串不表示“没有采集”。

`AllocationStats::available_fields` 是 `StatField` 位掩码。某一位未置位时，对应的 `size_t` 不是 0 这个测量值。这是对设计中“不可提供的字段必须标记”的实现，因为原结构只有裸计数器。

`BuildRecord::source_revision` 在 `git rev-parse` 失败时为 `unavailable`，原因写入 `source_revision_reason`。

一次运行的标准输出协议：

- `SYSLAB_RESULT status=pass|fail|unsupported`
- `SYSLAB_METRIC name=value`
- `SYSLAB_NOTE basis=CLASS text=...`
- `SYSLAB_STEP text`：证明过程中的一步观测。页面按打印顺序展示这些行。

manifest 检查类型只有 `exit_code`、`stdout_contains`、`stderr_contains` 和 `result_marker`。
