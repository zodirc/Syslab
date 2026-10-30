# 贡献说明

## 构建与检查

修改 C++ 后，用 Clang 重新配置并编译 Debug 构建，然后运行 `syslab_tests`。改动实验运行器、manifest 或某个实验驱动时，对受影响的实验执行 `syslab validate` 和 `syslab run`。不要只编译通过就认为行为已经验证。

GCC 8 不能作为 C++20 编译器。引入只在更新标准库中存在的头文件时，需要同时考虑 `third_party/polyfill`，或者在文档里写明新的编译器要求。

## 实验

每个实验目录包含 `experiment.yaml`、`main.cpp`、`README.md` 和 `expected/behavior.md`。

- manifest 的 `id` 形如 `AT-003`。
- 通过条件必须是程序能够判定的事实，并写进 `expected/behavior.md`。
- 反汇编、系统调用和耗时是观察。观察不能当作语言或 ABI 的必然结论，除非该实验明确检查了对应数值。
- 主机缺少能力时输出 `SYSLAB_RESULT status=unsupported`，不要编造计数器或跟踪。
- 风险等级 `kernel_privileged` 与 `bare_metal` 不会在日常主机上执行。

## 注释与错误

对外头文件中的类型和函数使用 Doxygen 的 `@brief`、`@param`、`@return`、`@throws`。错误用 `syslab::Error` 和明确的 `Error::Code` 表示。分配器、锁和无锁结构的注释要写明它们不是生产实现，以及进度或公平性所依赖的条件。

## 提交

不要提交 `build/` 和 `artifacts/`。不要把一次运行的最优耗时写进文档，把它留在该次报告里。
