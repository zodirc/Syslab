#pragma once

#include <cstdint>
#include <string>

namespace syslab {

/**
 * @brief Returns the current UTC time as an ISO-8601 timestamp.
 * 返回当前 UTC 时间的 ISO-8601 时间戳。
 * @return Timestamp with second resolution, for example 2026-10-30T06:40:00Z.
 * 精确到秒的时间戳，例如 2026-10-30T06:40:00Z。
 */
std::string utc_timestamp();

/**
 * @brief Reads a monotonic clock in nanoseconds.
 * 读取单调时钟，单位为纳秒。
 * @return Nanoseconds since an unspecified epoch. Suitable for durations only.
 * 自某个未指定起点起的纳秒数。只适合计算时间间隔。
 */
std::uint64_t steady_nanoseconds();

/**
 * @brief Builds an identifier that is unique within this process and host clock.
 * 生成在本进程和主机时钟范围内唯一的标识。
 * @param prefix Short ASCII prefix such as "run" or "build".
 * 短 ASCII 前缀，例如 "run" 或 "build"。
 * @return Identifier containing the UTC time and random hex.
 * 包含 UTC 时间和随机十六进制的标识。
 */
std::string make_id(const std::string& prefix);

}  // namespace syslab
