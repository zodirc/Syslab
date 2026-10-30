# 0003. 用状态位表示不可用的测量

## 状态

已接受。

## 背景

设计中的 `AllocationStats` 和 `EnvironmentSnapshot` 使用普通数值或字符串，同时又禁止把采不到的数据写成看似有效的 0 或空字符串。

## 决定

`AllocationStats` 增加 `available_fields`。`EnvironmentSnapshot` 的主机事实使用 `SampledField`，包含状态、值和原因。构建记录在没有 git 修订时写入记号 `unavailable` 和原因，而不是空字符串。

资源限制导致的结束不新增生命周期状态。它记录在 `failure_class`：`timed_out`、`output_limit`、`resource_limit`、`build`、`verification`、`unsupported`、`interrupted`。

## 后果

读取统计或环境的代码必须先看状态或掩码。报告生成也按这个规则输出。
