# 数据模型

公开结构定义在 `include/syslab/experiment/model.hpp`。

`ExperimentState` 使用设计中的 12 个状态名。`RiskLevel` 的文本是 `readonly`、`normal`、`resource_intensive`、`kernel_privileged`、`bare_metal`。

`SampledField` 表示一次采集。`status` 为 `Ok`、`Unavailable`、`Unsupported` 或 `Failed`。调用方先看状态；状态不是 `Ok` 时，`value` 不是测量结果。空字符串不表示“没有采集”。

`AllocationStats::available_fields` 是 `StatField` 位掩码。某一位未置位时，对应的 `size_t` 不是 0 这个测量值。这是对设计中“不可提供的字段必须标记”的实现，因为原结构只有裸计数器。

`BuildRecord::source_revision` 在 `git rev-parse` 失败时为 `unavailable`，原因写入 `source_revision_reason`。

`build.json` 把 `compile_arguments` 和 `link_arguments` 写成 JSON 数组，元素与传给编译器的参数数组相同。报告第 7 节逐条列出这两组参数，并输出由它们拼接而成的命令行。

`source_dirty` 是对象。`status` 为 `ok` 时，`value` 表示 `git status --porcelain` 是否产生了输出。`status` 不是 `ok` 时没有 `value`，原因在 `reason`。`source_digest` 为 64 位小写十六进制，或字面量 `unavailable`。后者不是摘要，原因在 `source_digest_reason`。摘要覆盖清单列出的源文件，以及编译器以 `-MD` 写出的依赖文件中的前提路径。路径先排序并去重，再按 8 字节大端长度、路径字节、8 字节大端长度、文件字节的顺序拼接，最后计算 SHA-256。依赖文件缺失或某个前提路径无法读取时，摘要为 `unavailable`。

变体链接 `libsyslab.a` 时，`library_path`、`library_build_type` 和 `library_digest` 记录该归档的路径、CMake 构建类型和 SHA-256。Debug 优先使用命令行旁边的 `libsyslab.a`，否则使用 `build/debug/libsyslab.a`。Release 与 RelWithDebInfo 分别使用 `build/release` 和 `build/relwithdebinfo` 下的归档；文件不存在时由构建管理器调用 CMake 生成。变体不链接该库时，这三个字段都是字面量 `unavailable`，`library_reason` 为 `manifest does not link libsyslab.a`。`cache_hit` 为 true 时，`cache_build_id` 是被复用的那次构建标识，当前记录仍有新的 `build_id`。缓存目录是 `artifacts/cache/<前缀>/<完整键>/entry.json`。`--no-cache` 跳过查找，该次成功编译也不写入缓存项。`trim_build_cache` 先删除可执行文件已经不存在的项，再按 `build_id` 删除最旧项，直到剩余项不超过 64。`syslab clean` 会把仍被保留构建引用的可执行文件传给这次裁剪，这些项保留且不计入 64 的上限。

`EnvironmentSnapshot` 还包含 `cpufreq_governor`、`load_average` 和 `cpu_affinity`。governor 读取 `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`。全部可读 CPU 的值相同时记录这个共同值，不同时按 CPU 列出。该文件不存在时状态为 unavailable，原因是 `scaling_governor is not exposed`。`load_average` 是 `/proc/loadavg` 的前三个字段。`cpu_affinity` 来自运行进程的 `sched_getaffinity`。运行器不调用 `sched_setaffinity`，所以这是继承的掩码，范围限于 `CPU_SETSIZE`。

`ObservationRecord` 有 `tool_path`。外部工具先经 `which` 解析，再运行解析后的路径取得版本首行。`tool_version` 是该输出的第一行。命令不存在时 `tool_path` 为空，`tool_version` 为 `unavailable`。

清单的 `observe` 含 `performance_counter` 时必须提供非空的 `perf_events`。事件名只允许字母、数字和 `_.:/-`。能力 `perf_events` 在 `perf` 不可用时为 unavailable。可用时 `version` 是 `/proc/sys/kernel/perf_event_paranoid` 的内容，并且只有 `perf stat -e cycles -- /bin/true` 以 0 退出才是 ok。采集命令是 `perf stat -x, -e <事件> -o <run-id>.perf.csv --`。同时要求 `strace` 时，`strace` 包在 `perf` 外面。CSV 没有样本时观测不完整。报告第 12 节只打印事件名和计数值。

`result_marker` 使用 `parse_protocol` 读取标准输出。重复的 `SYSLAB_RESULT`、无法识别的状态或依据、缺少 `name=value` 的度量、空的 `SYSLAB_STEP`，以及未知的 `SYSLAB_` 前缀，都会使该检查失败。报告第 7 节列出库路径、构建类型、摘要和缓存命中。报告第 12 节列出协议中的度量，并写出格式错误。只有一次运行时写明样本数为 1，不输出离散程度。多于一次时，整数度量给出次数、最小值、中位数、最大值和四分位距。偶数个样本的中位数是排序后下标 `(n - 1) / 2` 的值。四分位取下标 `(n - 1) / 4` 和 `3 * (n - 1) / 4`，四分位距是二者之差。所选变体的 `build_type` 为 Debug 时，第 12 节包含“调试构建，不作性能结论”。编译参数包含 `-fsanitize` 时，若清单没有产生 `sanitizer_report` 观测，会话追加一条 `SanitizerReport`，工件是该次运行的标准错误。清单要求 `sanitizer_report` 但编译参数没有 `-fsanitize` 时，该观测的 `complete` 为 false，`limitation` 为 `this build was not compiled with a sanitizer`。标准输出或标准错误文件打不开时，对应观测不完整，内存里的管道内容仍可用于检查。

一次运行的标准输出协议：

- `SYSLAB_RESULT status=pass|fail|unsupported`
- `SYSLAB_METRIC name=value`
- `SYSLAB_NOTE basis=CLASS text=...`
- `SYSLAB_STEP text`：证明过程中的一步观测。页面按打印顺序展示这些行。

manifest 检查类型只有 `exit_code`、`stdout_contains`、`stderr_contains` 和 `result_marker`。
