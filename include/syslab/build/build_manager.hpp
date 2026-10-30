#pragma once

#include "syslab/experiment/manifest.hpp"

#include <string>

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

}  // namespace syslab
