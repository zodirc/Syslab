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

}  // namespace syslab
