#pragma once

#include "syslab/core/json.hpp"
#include "syslab/experiment/model.hpp"

#include <string>

namespace syslab {

/**
 * @brief Collects the host facts required by an environment snapshot.
 * 收集环境快照所需的主机事实。
 * @param compiler_path Compiler used for the associated build. May be empty.
 * 相关构建使用的编译器。可以为空。
 * @return Snapshot. Individual fields may be unavailable.
 * 快照。单个字段可以是不可用。
 */
EnvironmentSnapshot capture_environment(const std::string& compiler_path);

/**
 * @brief Serializes a snapshot.
 * 序列化一份快照。
 * @param snapshot Environment snapshot.
 * 环境快照。
 * @return JSON object. Each sampled field carries status, value, and reason.
 * JSON 对象。每个采样字段带有 status、value 和 reason。
 */
Json environment_to_json(const EnvironmentSnapshot& snapshot);

}  // namespace syslab
