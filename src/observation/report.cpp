#include "syslab/observation/report.hpp"

#include "syslab/core/fs.hpp"
#include "syslab/observation/protocol.hpp"
#include "syslab/platform/environment.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <sstream>
#include <vector>

namespace syslab {
namespace {

void section(std::ostringstream* out, int number, const char* title) {
  *out << "\n## " << number << ". " << title << "\n\n";
}

std::string quote_argument(const std::string& argument) {
  if (argument.empty()) {
    return "''";
  }
  bool plain = true;
  for (unsigned char character : argument) {
    const bool safe = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                      (character >= '0' && character <= '9') || character == '/' || character == '.' ||
                      character == '_' || character == '+' || character == '-' || character == '=' || character == ',';
    if (!safe) {
      plain = false;
      break;
    }
  }
  if (plain) {
    return argument;
  }
  std::string quoted = "'";
  for (char character : argument) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

void write_argument_list(std::ostringstream* out, const char* label, const std::vector<std::string>& arguments) {
  *out << label << "\n";
  if (arguments.empty()) {
    *out << "(empty)\n\n";
    return;
  }
  for (const std::string& argument : arguments) {
    *out << argument << "\n";
  }
  *out << "\n";
}

}  // namespace

namespace {

bool parse_metric_integer(const std::string& text, std::int64_t* value) {
  if (text.empty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long long parsed = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
    return false;
  }
  *value = static_cast<std::int64_t>(parsed);
  return true;
}

std::vector<std::string> split_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  for (char character : line) {
    if (character == ',') {
      fields.push_back(field);
      field.clear();
    } else if (character != '\r') {
      field.push_back(character);
    }
  }
  fields.push_back(field);
  return fields;
}

}  // namespace

std::vector<MetricSummary> summarize_metrics(const std::vector<MetricSample>& samples) {
  std::vector<std::string> order;
  std::map<std::string, std::vector<std::string>> grouped;
  for (const MetricSample& sample : samples) {
    if (grouped.find(sample.name) == grouped.end()) {
      order.push_back(sample.name);
    }
    grouped[sample.name].push_back(sample.value);
  }
  std::vector<MetricSummary> summaries;
  for (const std::string& name : order) {
    MetricSummary summary;
    summary.name = name;
    summary.values = grouped[name];
    summary.count = summary.values.size();
    std::vector<std::int64_t> numbers;
    bool integer = !summary.values.empty();
    for (const std::string& value : summary.values) {
      std::int64_t number = 0;
      if (!parse_metric_integer(value, &number)) {
        integer = false;
        break;
      }
      numbers.push_back(number);
    }
    summary.integer = integer;
    if (integer) {
      std::sort(numbers.begin(), numbers.end());
      const std::size_t count = numbers.size();
      summary.minimum = numbers.front();
      summary.maximum = numbers.back();
      summary.lower_median = numbers[(count - 1) / 2];
      const std::int64_t first_quartile = numbers[(count - 1) / 4];
      const std::int64_t third_quartile = numbers[(3 * (count - 1)) / 4];
      summary.interquartile_range = third_quartile - first_quartile;
    }
    summaries.push_back(std::move(summary));
  }
  return summaries;
}

std::vector<PerfSample> parse_perf_csv(const std::string& text) {
  std::vector<PerfSample> samples;
  std::size_t begin = 0;
  while (begin < text.size()) {
    std::size_t end = text.find('\n', begin);
    if (end == std::string::npos) {
      end = text.size();
    }
    const std::string line = text.substr(begin, end - begin);
    begin = end + 1;
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<std::string> fields = split_csv_line(line);
    if (fields.size() < 3 || fields[2].empty()) {
      continue;
    }
    PerfSample sample;
    sample.value = fields[0];
    sample.event = fields[2];
    samples.push_back(std::move(sample));
  }
  return samples;
}

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
  if (input.build.source_dirty_known) {
    out << "source_dirty: " << (input.build.source_dirty ? "true" : "false") << "\n\n";
  } else {
    out << "source_dirty: unavailable\n\n";
    if (!input.build.source_dirty_reason.empty()) {
      out << "source_dirty_reason: " << input.build.source_dirty_reason << "\n\n";
    }
  }
  out << "source_digest: " << input.build.source_digest << "\n\n";
  if (!input.build.source_digest_reason.empty()) {
    out << "source_digest_reason: " << input.build.source_digest_reason << "\n\n";
  }
  out << "artifact_digest: " << input.build.artifact_digest << "\n\n";
  out << "library_path: " << input.build.library_path << "\n\n";
  out << "library_build_type: " << input.build.library_build_type << "\n\n";
  out << "library_digest: " << input.build.library_digest << "\n\n";
  if (!input.build.library_reason.empty()) {
    out << "library_reason: " << input.build.library_reason << "\n\n";
  }
  out << "cache_hit: " << (input.build.cache_hit ? "true" : "false") << "\n\n";
  if (!input.build.cache_build_id.empty()) {
    out << "cache_build_id: " << input.build.cache_build_id << "\n\n";
  }
  write_argument_list(&out, "compile_arguments:", input.build.compile_arguments);
  write_argument_list(&out, "link_arguments:", input.build.link_arguments);
  out << "command:\n";
  std::vector<std::string> command = input.build.compile_arguments;
  command.insert(command.end(), input.build.link_arguments.begin(), input.build.link_arguments.end());
  for (std::size_t index = 0; index < command.size(); ++index) {
    if (index != 0) {
      out << " ";
    }
    out << quote_argument(command[index]);
  }
  out << "\n";
  section(&out, 8, "执行步骤");
  out << "运行记录数: " << input.runs.size() << "\n";
  section(&out, 9, "原始观测数据");
  if (input.observations.empty()) {
    out << "没有观测记录。\n";
  }
  for (const ObservationRecord& observation : input.observations) {
    out << "- " << to_string(observation.kind) << " tool=" << observation.tool_name;
    if (!observation.tool_path.empty()) {
      out << " path=" << observation.tool_path;
    }
    out << " version=" << observation.tool_version
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
  out << "性能数字只来自实验程序打印的 SYSLAB_METRIC 行和 perf stat 的 CSV。报告器不计算排名，也不解释原因。\n\n";
  if (input.variant_build_type == "Debug") {
    out << "调试构建，不作性能结论。\n\n";
  }
  bool saw_metric = false;
  std::vector<MetricSample> samples;
  for (const RunRecord& run : input.runs) {
    if (run.stdout_artifact.empty() || !path_exists(run.stdout_artifact)) {
      out << "- " << run.run_id << " 的标准输出不可用。\n";
      continue;
    }
    const ProtocolRecord protocol = parse_protocol(read_file(run.stdout_artifact));
    for (const ProtocolMetric& metric : protocol.metrics) {
      out << "- " << run.run_id << " " << metric.name << "=" << metric.value << "\n";
      samples.push_back(MetricSample {metric.name, metric.value});
      saw_metric = true;
    }
    for (const ProtocolDiagnostic& error : protocol.errors) {
      out << "- " << run.run_id << " protocol line " << error.line << ": " << error.message << "\n";
    }
  }
  if (!saw_metric) {
    out << "本次运行没有 SYSLAB_METRIC 行。\n";
  }
  if (input.runs.size() <= 1) {
    out << "样本数为 1，不输出离散程度。\n";
  } else {
    for (const MetricSummary& summary : summarize_metrics(samples)) {
      out << "- " << summary.name << " n=" << summary.count;
      if (summary.integer) {
        out << " min=" << summary.minimum << " median=" << summary.lower_median << " max=" << summary.maximum
            << " iqr=" << summary.interquartile_range;
      } else {
        out << " values=";
        for (std::size_t index = 0; index < summary.values.size(); ++index) {
          if (index != 0) {
            out << ",";
          }
          out << summary.values[index];
        }
      }
      out << "\n";
    }
  }
  for (const ObservationRecord& observation : input.observations) {
    if (observation.kind != ObservationKind::PerformanceCounter || observation.complete) {
      continue;
    }
    out << "性能计数器未采集";
    if (!observation.limitation.empty()) {
      out << "：" << observation.limitation;
    }
    out << "\n";
  }
  for (const RunRecord& run : input.runs) {
    if (run.stdout_artifact.size() < 7 ||
        run.stdout_artifact.compare(run.stdout_artifact.size() - 7, 7, ".stdout") != 0) {
      continue;
    }
    const std::string csv_path = run.stdout_artifact.substr(0, run.stdout_artifact.size() - 7) + ".perf.csv";
    if (!path_exists(csv_path)) {
      continue;
    }
    for (const PerfSample& sample : parse_perf_csv(read_file(csv_path))) {
      out << "- " << run.run_id << " perf " << sample.event << "=" << sample.value << "\n";
    }
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
