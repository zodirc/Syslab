#pragma once

#include "syslab/core/json.hpp"
#include "syslab/experiment/model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace syslab {

/**
 * @brief One compiler-flag configuration of an experiment.
 * 实验的一套编译器选项。
 */
struct VariantSpec {
  std::string name;
  std::string build_type;
  std::vector<std::string> flags;
};

/**
 * @brief Resource ceilings declared by a manifest.
 * 清单声明的资源上限。
 *
 * Zero memory_bytes or cpu_seconds selects the runner default for the risk
 * level. timeout_ms and output_bytes are always concrete after validation.
 * memory_bytes 或 cpu_seconds 为 0 时，使用该风险等级的运行器默认值。校验之后 timeout_ms 和 output_bytes 一定是具体数值。
 */
struct LimitSpec {
  std::uint64_t timeout_ms = 10000;
  std::uint64_t output_bytes = 1048576;
  std::uint64_t memory_bytes = 0;
  std::uint64_t cpu_seconds = 0;
  std::uint64_t max_threads = 64;
};

/**
 * @brief One verification rule.
 * 一条验证规则。
 */
struct CheckSpec {
  std::string name;
  std::string type;
  std::string expected;
};

/**
 * @brief Validated experiment manifest.
 * 已校验的实验清单。
 *
 * The directory is the manifest location. Source paths are relative to it and
 * cannot escape it.
 * 目录是清单所在位置。源码路径相对该目录，并且不能逃出它。
 */
struct Manifest {
  ExperimentDescriptor experiment;
  std::string os;
  std::vector<std::string> architectures;
  std::string target;
  std::string standard;
  std::vector<std::string> sources;
  std::vector<std::string> link_libraries;
  std::vector<VariantSpec> variants;
  std::string executable;
  std::vector<std::string> arguments;
  int repetitions = 1;
  LimitSpec limits;
  std::vector<std::string> observe;
  std::vector<CheckSpec> checks;
  std::vector<std::string> preserve;
  std::vector<std::string> prerequisites;
  std::vector<std::string> concepts;
  std::vector<std::string> basis;
  std::string directory;
  std::string manifest_path;
};

/**
 * @brief Loads and validates a manifest file.
 * 加载并校验一份清单文件。
 * @param path Path to experiment.yaml.
 * experiment.yaml 的路径。
 * @return Validated manifest.
 * 已校验的清单。
 * @throws Error Parse when the YAML subset is violated.
 * 违反 YAML 子集时抛出 Error Parse。
 * @throws Error Validation when a required field or constraint fails.
 * 必填字段或约束失败时抛出 Error Validation。
 * @throws Error Io when the file cannot be read.
 * 文件无法读取时抛出 Error Io。
 */
Manifest load_manifest(const std::string& path);

/**
 * @brief Finds a variant by name.
 * 按名称查找变体。
 * @param manifest Manifest that owns the variants.
 * 拥有这些变体的清单。
 * @param name Variant name. An empty name selects the first variant.
 * 变体名。空名称选择第一个变体。
 * @return Matching variant.
 * 匹配的变体。
 * @throws Error NotFound when the name is unknown.
 * 名称未知时抛出 Error NotFound。
 */
const VariantSpec& find_variant(const Manifest& manifest, const std::string& name);

/**
 * @brief Serializes a manifest summary as JSON.
 * 把清单摘要序列化为 JSON。
 * @param manifest Validated manifest.
 * 已校验的清单。
 * @return JSON object.
 * JSON 对象。
 */
Json manifest_to_json(const Manifest& manifest);

}  // namespace syslab
