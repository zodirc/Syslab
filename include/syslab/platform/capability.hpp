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
 * @brief Probes tools and host facilities used by observations.
 * 探测观察所用的工具和主机设施。
 * @return One record per probed capability. Missing tools are unavailable, not invented.
 * 每个被探测能力一条记录。缺失的工具标为 unavailable，不编造。
 */
std::vector<Capability> probe_capabilities();

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
