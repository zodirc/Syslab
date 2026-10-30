#pragma once

#include "syslab/core/error.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace syslab {

/**
 * @brief Adds two sizes or reports an overflow.
 * 把两个大小相加，溢出时报告失败。
 * @param left First operand.
 * 第一个操作数。
 * @param right Second operand.
 * 第二个操作数。
 * @param out Receives the sum when the addition fits.
 * 加法能够表示时，接收和。
 * @return True when the sum cannot be represented.
 * 和无法表示时为真。
 */
inline bool add_overflow(std::size_t left, std::size_t right, std::size_t* out) noexcept {
  if (left > std::numeric_limits<std::size_t>::max() - right) {
    return true;
  }
  *out = left + right;
  return false;
}

/**
 * @brief Multiplies two sizes or reports an overflow.
 * 把两个大小相乘，溢出时报告失败。
 * @param left First operand.
 * 第一个操作数。
 * @param right Second operand.
 * 第二个操作数。
 * @param out Receives the product when it fits.
 * 乘积能够表示时，接收乘积。
 * @return True when the product cannot be represented.
 * 乘积无法表示时为真。
 */
inline bool mul_overflow(std::size_t left, std::size_t right, std::size_t* out) noexcept {
  if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
    return true;
  }
  *out = left * right;
  return false;
}

/**
 * @brief Checks that a requested alignment is a non-zero power of two.
 * 检查请求的对齐是否为非零的 2 的幂。
 * @param alignment Alignment in bytes.
 * 对齐字节数。
 * @return True when the alignment can be used by the allocators.
 * 分配器可以使用该对齐时为真。
 */
inline bool is_power_of_two(std::size_t alignment) noexcept {
  return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

/**
 * @brief Rounds a size up to a power-of-two alignment.
 * 把大小向上取整到 2 的幂对齐。
 * @param value Size or address value to align.
 * 要对齐的大小或地址。
 * @param alignment Power-of-two alignment. Zero and non-powers are rejected.
 * 2 的幂对齐。零和非 2 的幂会被拒绝。
 * @return Aligned value.
 * 对齐后的值。
 * @throws Error InvalidArgument when the alignment is illegal.
 * 对齐非法时抛出 Error InvalidArgument。
 * @throws Error InvalidArgument when rounding would overflow.
 * 取整会溢出时抛出 Error InvalidArgument。
 */
inline std::size_t align_up(std::size_t value, std::size_t alignment) {
  if (!is_power_of_two(alignment)) {
    throw Error(Error::Code::InvalidArgument, "alignment must be a non-zero power of two");
  }
  const std::size_t mask = alignment - 1;
  if (value > std::numeric_limits<std::size_t>::max() - mask) {
    throw Error(Error::Code::InvalidArgument, "aligned size overflows size_t");
  }
  return (value + mask) & ~mask;
}

/**
 * @brief Requires a predicate and throws when it is false.
 * 要求谓词成立，否则抛出异常。
 * @param condition Condition that must hold.
 * 必须成立的条件。
 * @param code Error code used when the condition is false.
 * 条件为假时使用的错误码。
 * @param message Context recorded on failure.
 * 失败时记录的上下文。
 * @throws Error When condition is false.
 * 条件为假时抛出 Error。
 */
inline void require(bool condition, Error::Code code, const char* message) {
  if (!condition) {
    throw Error(code, message);
  }
}

}  // namespace syslab
