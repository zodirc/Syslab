#pragma once

#include "syslab/experiment/manifest.hpp"

#include <cstdint>
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
  /**
   * Build type of the variant selected for this session, such as Debug or Release.
   * 本次会话所选变体的构建类型，例如 Debug 或 Release。
   */
  std::string variant_build_type;
  std::string readme;
  std::string behavior;
};

/**
 * @brief One named measurement collected from a protocol line.
 * 从协议行收集的一个具名测量。
 */
struct MetricSample {
  std::string name;
  std::string value;
};

/**
 * @brief Dispersion of one metric across repeated runs.
 * 一个度量在重复运行上的离散程度。
 *
 * When every value is an integer, minimum, lower_median, maximum, and
 * interquartile_range are populated. The median of an even count is the lower
 * of the two middle samples, at index (n - 1) / 2 after sorting. Quartile
 * indexes are (n - 1) / 4 and 3 * (n - 1) / 4. Non-integer values leave the
 * numeric fields at zero and integer false; values still lists the text.
 * 每个值都是整数时，填写 minimum、lower_median、maximum 和 interquartile_range。
 * 偶数个样本的中位数取排序后下标 (n - 1) / 2 的较低中间值。四分位下标是 (n - 1) / 4 和 3 * (n - 1) / 4。
 * 含非整数时数值字段保持 0 且 integer 为假；values 仍列出原文。
 */
struct MetricSummary {
  std::string name;
  std::size_t count = 0;
  bool integer = false;
  std::int64_t minimum = 0;
  std::int64_t lower_median = 0;
  std::int64_t maximum = 0;
  std::int64_t interquartile_range = 0;
  std::vector<std::string> values;
};

/**
 * @brief One row of a perf stat CSV file produced with -x,.
 * perf stat 使用 -x, 写出的 CSV 中的一行。
 */
struct PerfSample {
  std::string event;
  std::string value;
};

/**
 * @brief Summarizes metric samples grouped by name, in first-seen order.
 * 按名称汇总度量样本，顺序为首次出现的顺序。
 * @param samples Metric name and value pairs.
 * 度量名称和值。
 * @return One summary per distinct name.
 * 每个不同名称一条汇总。
 */
std::vector<MetricSummary> summarize_metrics(const std::vector<MetricSample>& samples);

/**
 * @brief Parses perf stat CSV text. Comment lines starting with # are skipped.
 * 解析 perf stat 的 CSV 文本。以 # 开头的注释行被跳过。
 *
 * A data row uses the -x, layout value,unit,event. The event is the third
 * field and the count is the first field. No event name is translated.
 * 数据行采用 -x, 布局 value,unit,event。事件是第三列，计数是第一列。不翻译事件名。
 * @param text File contents.
 * 文件内容。
 * @return Samples in file order.
 * 按文件顺序的样本。
 */
std::vector<PerfSample> parse_perf_csv(const std::string& text);

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
