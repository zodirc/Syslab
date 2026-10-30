#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace syslab {

/**
 * @brief Failure raised by the experiment framework and teaching implementations.
 * 实验框架和教学实现抛出的失败。
 *
 * The code identifies the failing stage. The message retains the immediate context
 * needed to locate that stage. Framework APIs use this type instead of status codes.
 * 错误码标明失败阶段。消息保留定位该阶段所需的直接上下文。框架接口使用该类型，而不是状态码。
 */
class Error : public std::runtime_error {
 public:
  enum class Code {
    InvalidArgument,
    NotFound,
    Io,
    Parse,
    Validation,
    Build,
    Execution,
    Unsupported,
    Timeout,
    ResourceLimit,
    State,
    Corruption,
    Logic
  };

  /**
   * @brief Creates an error with a stable code and a context message.
   * 用稳定错误码和上下文消息构造错误。
   * @param code Failure category.
   * 失败类别。
   * @param message Human-readable context. Must not contain secrets.
   * 给人读的上下文。不得包含秘密。
   */
  Error(Code code, std::string message)
      : std::runtime_error(std::move(message)), code_(code) {}

  /**
   * @brief Returns the failure category.
   * 返回失败类别。
   * @return Stable code for the failing stage.
   * 失败阶段的稳定错误码。
   */
  Code code() const noexcept { return code_; }

 private:
  Code code_;
};

/**
 * @brief Returns the stable name of an error code.
 * 返回错误码的稳定名称。
 * @param code Error category.
 * 错误类别。
 * @return Lower-case identifier used in JSON records.
 * JSON 记录里使用的小写标识。
 */
const char* to_string(Error::Code code) noexcept;

}  // namespace syslab
