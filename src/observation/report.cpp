#include "syslab/observation/report.hpp"

#include "syslab/core/fs.hpp"
#include "syslab/platform/environment.hpp"

#include <sstream>

namespace syslab {
namespace {

void section(std::ostringstream* out, int number, const char* title) {
  *out << "\n## " << number << ". " << title << "\n\n";
}

}  // namespace

std::string render_report(const ReportInput& input) {
  std::ostringstream out;
  const Manifest& manifest = *input.manifest;
  out << "# " << manifest.experiment.id << " " << manifest.experiment.name << "\n";
  section(&out, 1, "实验目的");
  out << manifest.experiment.description << "\n";
  section(&out, 2, "对应知识点");
  if (manifest.concepts.empty()) {
    out << "清单未声明概念标识。\n";
  }
  for (const std::string& concept_name : manifest.concepts) {
    out << "- " << concept_name << "\n";
  }
  section(&out, 3, "理论模型");
  out << (input.readme.empty() ? "实验目录没有 README。\n" : input.readme) << "\n";
  section(&out, 4, "关键术语与规范依据");
  if (manifest.basis.empty()) {
    out << "清单未声明依据类别。\n";
  }
  for (const std::string& basis : manifest.basis) {
    out << "- " << basis << "\n";
  }
  section(&out, 5, "实验环境");
  out << "environment_id: " << input.environment.environment_id << "\n\n";
  out << "环境字段带有采集状态。status 不是 ok 时，value 不是测量结果。\n\n";
  out << environment_to_json(input.environment).dump() << "\n";
  section(&out, 6, "实现说明");
  out << "实验源码位于 `" << manifest.directory << "`。\n";
  section(&out, 7, "构建参数");
  out << "build_id: " << input.build.build_id << "\n\n";
  out << "compiler: " << input.build.compiler_path << "\n\n";
  out << "compiler_version: " << input.build.compiler_version << "\n\n";
  out << "target: " << input.build.target_triple << "\n\n";
  out << "standard: " << input.build.language_standard << "\n\n";
  out << "source_revision: " << input.build.source_revision << "\n\n";
  if (!input.build.source_revision_reason.empty()) {
    out << "source_revision_reason: " << input.build.source_revision_reason << "\n\n";
  }
  out << "artifact_digest: " << input.build.artifact_digest << "\n";
  section(&out, 8, "执行步骤");
  out << "运行记录数: " << input.runs.size() << "\n";
  section(&out, 9, "原始观测数据");
  if (input.observations.empty()) {
    out << "没有观测记录。\n";
  }
  for (const ObservationRecord& observation : input.observations) {
    out << "- " << to_string(observation.kind) << " tool=" << observation.tool_name
        << " complete=" << (observation.complete ? "true" : "false");
    if (!observation.limitation.empty()) {
      out << " limitation=" << observation.limitation;
    }
    out << " digest=" << observation.content_digest << "\n";
  }
  section(&out, 10, "汇编或系统调用分析");
  out << "本节只列出工具产物路径。工具输出本身不是因果结论。\n\n";
  for (const ObservationRecord& observation : input.observations) {
    if (observation.kind == ObservationKind::Disassembly || observation.kind == ObservationKind::SyscallTrace ||
        observation.kind == ObservationKind::ElfMetadata) {
      out << "- " << observation.artifact_path << "\n";
    }
  }
  section(&out, 11, "正确性验证");
  for (const VerificationResult& check : input.checks) {
    out << "- " << check.check_id << " " << to_string(check.status) << " expected=" << check.expected
        << " actual=" << check.actual << "\n";
    if (!check.explanation.empty()) {
      out << "  " << check.explanation << "\n";
    }
  }
  section(&out, 12, "性能结果");
  out << "性能数字只来自实验程序打印的 SYSLAB_METRIC 行。报告器不计算排名，也不解释原因。\n\n";
  bool saw_metric = false;
  for (const RunRecord& run : input.runs) {
    if (run.stdout_artifact.empty() || !path_exists(run.stdout_artifact)) {
      out << "- " << run.run_id << " 的标准输出不可用。\n";
      continue;
    }
    const std::string text = read_file(run.stdout_artifact);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
      const std::size_t end = text.find('\n', cursor);
      const std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
      if (line.compare(0, 14, "SYSLAB_METRIC ") == 0) {
        out << "- " << run.run_id << " " << line.substr(14) << "\n";
        saw_metric = true;
      }
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 1;
    }
  }
  if (!saw_metric) {
    out << "本次运行没有 SYSLAB_METRIC 行。\n";
  }
  section(&out, 13, "现象解释");
  out << (input.behavior.empty() ? "实验目录没有 expected/behavior.md。\n" : input.behavior) << "\n";
  section(&out, 14, "与理论不一致的地方");
  out << "报告器不根据单次结果宣布理论失效。若验证状态为 failed，失败记录保留在第 11 节。\n";
  section(&out, 15, "平台和实现限制");
  out << "failure_class: " << (input.failure_class.empty() ? "none" : input.failure_class) << "\n\n";
  out << "状态 " << to_string(input.state) << " 只描述本次调用。它不推广到其他编译器、内核或微架构。\n";
  section(&out, 16, "结论");
  out << "本次调用的生命周期状态是 " << to_string(input.state) << "。\n";
  section(&out, 17, "参考资料");
  out << "依据类别见第 4 节。资料条目见 docs/references/sources.md。\n";
  return out.str();
}

}  // namespace syslab
