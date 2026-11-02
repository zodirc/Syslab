#pragma once

#include "syslab/core/json.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Availability of one external tool or host facility.
 * 一个外部工具或主机设施的可用性。
 */
struct Capability {
  std::string name;
  std::string status;
  std::string version;
  std::string reason;
};

/**
 * @brief One external tool resolved through PATH.
 * 经 PATH 解析的一个外部工具。
 *
 * path is empty and version is the literal unavailable when which cannot
 * resolve the command. available is true only after the version command exits 0.
 * which 无法解析命令时 path 为空，version 为字面量 unavailable。仅当版本命令以 0 退出时 available 为真。
 */
struct ResolvedTool {
  std::string name;
  std::string path;
  std::string version = "unavailable";
  std::string reason;
  bool available = false;
};

/**
 * @brief Probes tools and host facilities used by observations.
 * 探测观察所用的工具和主机设施。
 * @return One record per probed capability. Missing tools are unavailable, not invented.
 * 每个被探测能力一条记录。缺失的工具标为 unavailable，不编造。
 */
std::vector<Capability> probe_capabilities();

/**
 * @brief Resolves a tool with which, then runs that path for its version line.
 * 用 which 解析工具，再运行该路径以取得版本行。
 * @param name Executable name passed to which.
 * 传给 which 的可执行文件名。
 * @param version_args Arguments appended to the resolved path, such as --version or -V.
 * 追加到解析路径后的参数，例如 --version 或 -V。
 * @return Path, first version line, and availability. A missing tool is not given a fake version.
 * 路径、版本首行和可用性。缺失的工具不会得到编造的版本。
 */
ResolvedTool resolve_tool(const std::string& name, const std::vector<std::string>& version_args);

/**
 * @brief Probes whether perf can sample the cycles event.
 * 探测 perf 能否采样 cycles 事件。
 *
 * When perf is unavailable the status is unavailable. Otherwise version is the
 * contents of perf_event_paranoid, and status is ok only after
 * perf stat -e cycles -- /bin/true exits 0.
 * perf 不可用时状态为 unavailable。否则 version 是 perf_event_paranoid 的内容，
 * 并且只有 perf stat -e cycles -- /bin/true 以 0 退出时状态才是 ok。
 * @param perf Tool already resolved with --version.
 * 已经用 --version 解析过的 perf。
 * @return Capability named perf_events.
 * 名称为 perf_events 的能力记录。
 */
Capability probe_perf_events(const ResolvedTool& perf);

/**
 * @brief Serializes capability records.
 * 序列化能力记录。
 * @param capabilities Probe results.
 * 探测结果。
 * @return JSON array.
 * JSON 数组。
 */
Json capabilities_to_json(const std::vector<Capability>& capabilities);

/**
 * @brief Finds a capability by name.
 * 按名称查找能力。
 * @param capabilities Probe results.
 * 探测结果。
 * @param name Capability name.
 * 能力名称。
 * @return Matching record, or null when the name was not probed.
 * 匹配的记录。该名称未被探测时为空。
 */
const Capability* find_capability(const std::vector<Capability>& capabilities, const std::string& name);

}  // namespace syslab
