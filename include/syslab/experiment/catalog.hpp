#pragma once

#include "syslab/experiment/registry.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Checks that experiment directories, the index, and the course agree.
 * 检查实验目录、索引和课程是否一致。
 *
 * Each registered experiment directory must contain experiment.yaml, main.cpp,
 * README.md, and expected/behavior.md. The identifier set in
 * docs/experiments/index.md must equal the registry. Every identifier in
 * docs/learn/course.txt must already be registered. The function collects every
 * mismatch instead of stopping at the first one.
 * 每个已注册实验目录必须包含 experiment.yaml、main.cpp、README.md 和
 * expected/behavior.md。docs/experiments/index.md 中的标识集合必须与注册表相同。
 * docs/learn/course.txt 中的每个标识必须已经注册。函数收集全部不一致项，不在第一处停止。
 *
 * @param source_root Repository root that contains experiments/ and docs/.
 * 包含 experiments/ 和 docs/ 的仓库根目录。
 * @param registry Registry loaded from source_root/experiments.
 * 从 source_root/experiments 加载的注册表。
 * @return Empty when the catalog is consistent. Otherwise one message per mismatch.
 * 目录一致时为空。否则每处不一致一条消息。
 * @throws Error Io when a required catalog file exists but cannot be read.
 * 必需的目录文件存在但无法读取时抛出 Error Io。
 */
std::vector<std::string> check_catalog(const std::string& source_root, const Registry& registry);

}  // namespace syslab
