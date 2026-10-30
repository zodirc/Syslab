#pragma once

#include "syslab/experiment/manifest.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Inputs available to the report generator.
 * 报告生成器可用的输入。
 *
 * The generator copies environment, build, run, observation, and verification
 * records. Explanatory sections quote the experiment documents. The generator
 * does not infer a cause from a measurement.
 * 生成器复制环境、构建、运行、观察和验证记录。解释性章节引用实验文档。生成器不从测量推断原因。
 */
struct ReportInput {
  const Manifest* manifest = nullptr;
  EnvironmentSnapshot environment;
  BuildRecord build;
  std::vector<RunRecord> runs;
  std::vector<ObservationRecord> observations;
  std::vector<VerificationResult> checks;
  ExperimentState state = ExperimentState::Failed;
  std::string failure_class;
  std::string readme;
  std::string behavior;
};

/**
 * @brief Renders the fixed 17-section experiment report.
 * 渲染固定的 17 节实验报告。
 * @param input Records and experiment documents.
 * 记录和实验文档。
 * @return Markdown report.
 * Markdown 报告。
 */
std::string render_report(const ReportInput& input);

}  // namespace syslab
