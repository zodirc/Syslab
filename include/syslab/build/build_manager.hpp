#pragma once

#include "syslab/experiment/manifest.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Paths the build manager needs to compile an experiment.
 * 构建管理器编译实验所需的路径。
 */
struct BuildContext {
  std::string source_root;
  std::string compiler;
  std::string library_dir;
  std::string polyfill_dir;
  std::string output_directory;
  bool use_cache = true;
};

/**
 * @brief One compiled experiment variant.
 * 一个已编译的实验变体。
 */
struct BuiltExperiment {
  BuildRecord record;
  std::string executable;
  std::string directory;
  bool success = false;
  std::string message;
};

/**
 * @brief Compiles one experiment variant and records the invocation.
 * 编译一个实验变体并记录这次调用。
 *
 * The compiler is executed with a structured argument vector. Source paths are
 * taken from the manifest and cannot be shell text. The static syslab library
 * is linked when the manifest requests it.
 * 编译器通过结构化参数数组执行。源码路径来自清单，不能是 shell 文本。清单要求链接时会链接 syslab 静态库。
 *
 * @param manifest Validated manifest.
 * 已校验的清单。
 * @param variant Variant selected by the caller.
 * 调用者选中的变体。
 * @param context Compiler, include paths, and output directory.
 * 编译器、包含路径和输出目录。
 * @return Build result. success is false when the compiler exits non-zero.
 * 构建结果。编译器非零退出时 success 为假。
 * @throws Error Io when the output directory cannot be created.
 * 无法创建输出目录时抛出 Error Io。
 * @throws Error NotFound when the compiler or syslab library is missing.
 * 找不到编译器或 syslab 库时抛出 Error NotFound。
 */
BuiltExperiment build_experiment(const Manifest& manifest, const VariantSpec& variant,
                                 const BuildContext& context);

/**
 * @brief Reads prerequisite paths from a compiler dependency file.
 * 从编译器依赖文件读取前提路径。
 *
 * Line continuations and escaped spaces follow the Makefile rule syntax
 * emitted by Clang and GCC with -MD. The rule target is omitted. Tokens that
 * appear before the first colon are omitted.
 * 续行和转义空格遵循 Clang 与 GCC 使用 -MD 写出的 Makefile 规则语法。规则目标被略去。
 * 第一个冒号之前的记号被略去。
 *
 * @param text Dependency file contents.
 * 依赖文件内容。
 * @return Prerequisite paths in file order.
 * 按文件顺序排列的前提路径。
 */
std::vector<std::string> parse_make_dependencies(const std::string& text);

/**
 * @brief Computes one SHA-256 over a set of files.
 * 对一组文件计算一个 SHA-256。
 *
 * Paths are sorted and de-duplicated. Each record is an 8-byte big-endian path
 * length, the path bytes, an 8-byte big-endian content length, and the file
 * bytes. The result is the hex digest of that sequence.
 * 路径会排序并去重。每条记录依次是 8 字节大端路径长度、路径字节、8 字节大端内容长度和文件字节。
 * 结果是该序列的十六进制摘要。
 *
 * @param paths Files to include. May contain duplicates.
 * 要纳入的文件。可以含重复项。
 * @param reason Receives an empty string on success, or why the digest is unavailable.
 * 成功时收到空字符串，否则收到摘要不可用的原因。
 * @return Sixty-four lowercase hex characters, or the literal unavailable.
 * 64 位小写十六进制，或字面量 unavailable。
 * @throws Error InvalidArgument when reason is null.
 * reason 为空时抛出 Error InvalidArgument。
 */
std::string digest_files(const std::vector<std::string>& paths, std::string* reason);

/**
 * @brief Maximum number of reusable experiment build entries.
 * 可复用实验构建条目的数量上限。
 *
 * trim_build_cache deletes the oldest entries above this limit. The linked
 * libsyslab.a archives are not part of this count.
 * trim_build_cache 删除超过该上限的最旧条目。所链接的 libsyslab.a 不计入此数。
 */
constexpr std::size_t kBuildCacheLimit = 64;

/**
 * @brief Deletes oldest experiment build-cache entries above the limit.
 * 删除超过上限的最旧实验构建缓存条目。
 *
 * Entries whose executable file is already gone are removed first. The
 * remaining entries are ordered by build id, which begins with a UTC timestamp.
 * An entry whose executable path is listed in keep_executables is retained even
 * when the directory is already above the limit.
 * 可执行文件已经不存在的条目会先删除。其余条目按构建标识排序，该标识以 UTC 时间戳开头。
 * keep_executables 中列出的可执行文件所对应的条目即使超过上限也会保留。
 *
 * @param artifacts_dir Artifacts directory that contains cache/.
 * 包含 cache/ 的产物目录。
 * @param limit Maximum entries to retain, not counting keep_executables.
 * 保留的条目上限，不含 keep_executables。
 * @param keep_executables Executable paths that must stay reusable.
 * 必须保持可复用的可执行文件路径。
 * @return Number of cache entries removed.
 * 删除的缓存条目数。
 * @throws Error Io when a cache directory cannot be removed.
 * 缓存目录无法删除时抛出 Error Io。
 */
int trim_build_cache(const std::string& artifacts_dir, std::size_t limit,
                     const std::vector<std::string>& keep_executables = {});

}  // namespace syslab
