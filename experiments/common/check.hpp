#pragma once

#include <iostream>
#include <string>

/**
 * @brief Prints the pass marker and returns 0.
 * 打印通过标记并返回 0。
 * @return Process status 0.
 * 进程状态 0。
 */
inline int syslab_pass() {
  std::cout << "SYSLAB_RESULT status=pass\n";
  return 0;
}

/**
 * @brief Prints a failure marker.
 * 打印失败标记。
 * @param reason Short token without spaces.
 * 不含空格的短记号。
 * @return Process status 1.
 * 进程状态 1。
 */
inline int syslab_fail(const std::string& reason) {
  std::cout << "SYSLAB_RESULT status=fail reason=" << reason << "\n";
  return 1;
}

/**
 * @brief Prints an unsupported marker and returns 0 so the runner can classify it.
 * 打印不支持标记并返回 0，以便运行器把它归类。
 * @param reason Short token without spaces.
 * 不含空格的短记号。
 * @return Process status 0.
 * 进程状态 0。
 */
inline int syslab_unsupported(const std::string& reason) {
  std::cout << "SYSLAB_RESULT status=unsupported reason=" << reason << "\n";
  return 0;
}

/**
 * @brief Prints one integer metric.
 * 打印一个整数度量。
 * @param name Metric token.
 * 度量记号。
 * @param value Measured value.
 * 测得的值。
 */
inline void syslab_metric(const char* name, long long value) {
  std::cout << "SYSLAB_METRIC " << name << "=" << value << "\n";
}

/**
 * @brief Prints a note tagged with its authority class.
 * 打印带依据类别的说明。
 * @param basis LANG, ABI, COMPILER, OS, ISA, MICROARCH, or OBSERVED.
 * LANG、ABI、COMPILER、OS、ISA、MICROARCH 或 OBSERVED。
 * @param text Note text without newlines.
 * 不含换行的说明文本。
 */
inline void syslab_note(const char* basis, const std::string& text) {
  std::cout << "SYSLAB_NOTE basis=" << basis << " text=" << text << "\n";
}

/**
 * @brief Prints one observation in the proof sequence.
 * 打印证明过程中的一步观测。
 * @param text One line, without a newline.
 * 一行文本，不含换行。
 */
inline void syslab_step(const std::string& text) {
  std::cout << "SYSLAB_STEP " << text << "\n";
}
