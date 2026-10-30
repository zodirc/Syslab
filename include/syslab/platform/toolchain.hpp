#pragma once

#include "syslab/core/json.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief One compiler discovered on PATH.
 * 在 PATH 上发现的一个编译器。
 */
struct Toolchain {
  std::string id;
  std::string path;
  std::string version;
  std::string target;
  bool available = false;
  std::string reason;
};

/**
 * @brief Probes gcc, g++, clang, and clang++.
 * 探测 gcc、g++、clang 和 clang++。
 * @return One record per tool name. Absent tools are reported as unavailable.
 * 每个工具名一条记录。不存在的工具报告为 unavailable。
 */
std::vector<Toolchain> list_toolchains();

/**
 * @brief Probes one compiler by command name.
 * 按命令名探测一个编译器。
 * @param id Command name such as clang++ or gcc.
 * 命令名，例如 clang++ 或 gcc。
 * @return Toolchain record. available is false when the command cannot be executed.
 * 工具链记录。命令无法执行时 available 为假。
 */
Toolchain inspect_toolchain(const std::string& id);

/**
 * @brief Serializes toolchain records.
 * 序列化工具链记录。
 * @param toolchains Records to encode.
 * 要编码的记录。
 * @return JSON array.
 * JSON 数组。
 */
Json toolchains_to_json(const std::vector<Toolchain>& toolchains);

}  // namespace syslab
