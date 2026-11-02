#pragma once

#include "syslab/core/json.hpp"
#include "syslab/experiment/manifest.hpp"

#include <string>

namespace syslab {

/**
 * @brief Paths and overrides for one CLI invocation.
 * 一次命令行调用的路径和覆盖项。
 */
struct SessionConfig {
  std::string source_root;
  std::string compiler;
  std::string library_dir;
  std::string polyfill_dir;
  std::string artifacts_dir;
  std::string variant;
  int repeat = -1;
  bool use_cache = true;
};

/**
 * @brief Outcome of validate, build, or run.
 * validate、build 或 run 的结果。
 */
struct SessionOutput {
  ExperimentState state = ExperimentState::Failed;
  std::string failure_class;
  std::string message;
  std::string run_directory;
  Json summary = Json::object();
};

/**
 * @brief Validates a manifest against the host without building it.
 * 对照主机校验清单，但不编译。
 * @param manifest Manifest to check.
 * 要检查的清单。
 * @param config Paths used to locate the host description.
 * 用于定位主机描述的路径。
 * @return Unsupported or Passed. Passed here means the manifest can be attempted.
 * Unsupported 或 Passed。这里的 Passed 表示清单可以尝试运行。
 */
SessionOutput validate_session(const Manifest& manifest, const SessionConfig& config);

/**
 * @brief Builds one variant and writes a build record.
 * 构建一个变体并写入构建记录。
 * @param manifest Manifest to build.
 * 要构建的清单。
 * @param config Build paths and variant override.
 * 构建路径和变体覆盖。
 * @return Build outcome.
 * 构建结果。
 */
SessionOutput build_session(const Manifest& manifest, const SessionConfig& config);

/**
 * @brief Builds, runs, observes, verifies, and writes a report.
 * 构建、运行、观察、校验并写报告。
 *
 * R3 and R4 manifests are marked unsupported. A command-line flag cannot
 * change that decision. Each attempt gets a new run directory.
 * R3 和 R4 清单被标为不支持。命令行开关不能改变这个决定。每次尝试使用新的运行目录。
 *
 * @param manifest Manifest to execute.
 * 要执行的清单。
 * @param config Execution paths and optional repeat override.
 * 执行路径和可选的重复次数覆盖。
 * @return Terminal lifecycle outcome.
 * 终止生命周期结果。
 */
SessionOutput execute_session(const Manifest& manifest, const SessionConfig& config);

/**
 * @brief Counts removed by one clean_artifacts call.
 * 一次 clean_artifacts 调用删除的数量。
 */
struct CleanReport {
  int runs_removed = 0;
  int builds_removed = 0;
  int cache_removed = 0;
};

/**
 * @brief Keeps the newest run directories and the builds they reference.
 * 保留最新的运行目录以及它们引用的构建目录。
 *
 * Run directory names sort by their batch-YYYYMMDDTHHMMSS prefix. For each
 * experiment, the newest keep directories stay. Other run directories and
 * build directories that no remaining run references are deleted. index.jsonl
 * is rewritten from the kept runs. The build cache is trimmed separately;
 * executables still referenced by a kept build stay in the cache.
 * 运行目录名按 batch-YYYYMMDDTHHMMSS 前缀排序。每个实验保留最新的 keep 个目录。
 * 其余运行目录，以及没有任何保留运行引用的构建目录，会被删除。index.jsonl 按保留的运行重写。
 * 构建缓存单独裁剪；仍被保留构建引用的可执行文件留在缓存中。
 *
 * @param artifacts_dir Directory that contains runs/, builds/, cache/, and index.jsonl.
 * 包含 runs/、builds/、cache/ 和 index.jsonl 的目录。
 * @param keep Number of run directories to retain for each experiment.
 * 每个实验保留的运行目录数。
 * @return Counts of removed run directories, build directories, and cache entries.
 * 删除的运行目录、构建目录和缓存条目数量。
 * @throws Error Io when a directory cannot be removed or the index cannot be replaced.
 * 目录无法删除或索引无法替换时抛出 Error Io。
 */
CleanReport clean_artifacts(const std::string& artifacts_dir, std::size_t keep);

}  // namespace syslab
