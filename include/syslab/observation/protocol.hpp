#pragma once

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief One SYSLAB_METRIC line.
 * 一行 SYSLAB_METRIC。
 */
struct ProtocolMetric {
  std::string name;
  std::string value;
  int line = 0;
};

/**
 * @brief One SYSLAB_NOTE line with a valid basis.
 * 一行依据合法的 SYSLAB_NOTE。
 */
struct ProtocolNote {
  std::string basis;
  std::string text;
  int line = 0;
};

/**
 * @brief One protocol format error.
 * 一处协议格式错误。
 */
struct ProtocolDiagnostic {
  int line = 0;
  std::string message;
};

/**
 * @brief Structured experiment stdout protocol.
 * 结构化的实验标准输出协议。
 *
 * status is empty when no valid SYSLAB_RESULT was seen. Format errors are
 * retained even when a status was also parsed. Callers that verify a result
 * marker must fail when errors is not empty.
 * 没有看到合法的 SYSLAB_RESULT 时 status 为空。即使同时解析出了状态，格式错误也会保留。
 * 校验结果标记的调用方在 errors 非空时必须判为失败。
 */
struct ProtocolRecord {
  std::string status;
  std::string reason;
  std::vector<ProtocolMetric> metrics;
  std::vector<ProtocolNote> notes;
  std::vector<std::string> steps;
  std::vector<ProtocolDiagnostic> errors;
};

/**
 * @brief Parses experiment stdout into protocol records.
 * 把实验标准输出解析为协议记录。
 *
 * Recognized lines are SYSLAB_RESULT, SYSLAB_METRIC, SYSLAB_NOTE, and
 * SYSLAB_STEP. Other lines are ignored. A second result, an unknown status,
 * a metric without '=', an unknown basis, or an unknown SYSLAB_ prefix is a
 * format error. The first valid result is kept.
 * 识别 SYSLAB_RESULT、SYSLAB_METRIC、SYSLAB_NOTE 和 SYSLAB_STEP。其他行被忽略。
 * 第二个结果、未知状态、没有 '=' 的度量、未知依据或未知的 SYSLAB_ 前缀是格式错误。
 * 保留第一个合法结果。
 *
 * @param text Experiment standard output. Newlines may be LF or CRLF.
 * 实验标准输出。换行可以是 LF 或 CRLF。
 * @return Parsed record. errors is empty when every protocol line is valid.
 * 解析结果。每行协议都合法时 errors 为空。
 */
ProtocolRecord parse_protocol(const std::string& text);

}  // namespace syslab
