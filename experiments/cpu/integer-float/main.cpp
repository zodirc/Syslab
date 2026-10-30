#include "experiments/common/check.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
// 应用：序列化、网络报文和共享内存按字节保存整数与浮点。接收端读出的符号相反，或 1.0 变成其他数值时，先读取发送端内存中的字节。
// 原理：C++20 规定带符号整数使用补码。int 类型的 -1 在内存中每个字节都是 0xff。当 numeric_limits<float>::is_iec559 为真时，1.0f 的 binary32 编码是符号位 0、阶码 127、尾数 0，十六进制 0x3f800000。is_iec559 为假时，本节不改用推测编码。
int main() {
  const int negative = -1;
  const auto bytes = reinterpret_cast<const unsigned char*>(&negative);
  for (std::size_t index = 0; index < sizeof(negative); ++index) {
    if (bytes[index] != 0xff) {
    syslab_step("int -1 的某个字节不是 ff。");
    return syslab_fail("twos-complement");
  }
  }
  if (!std::numeric_limits<float>::is_iec559) {
    syslab_step("numeric_limits<float>::is_iec559 为假，本节不推测 1.0f 的编码。");
    return syslab_unsupported("float-not-iec559");
  }
  float one = 1.0f;
  std::uint32_t bits = 0;
  std::memcpy(&bits, &one, sizeof(bits));
  syslab_metric("float_one_bits", bits);
  const unsigned sign = bits >> 31;
  const unsigned exponent = (bits >> 23) & 0xffu;
  const unsigned fraction = bits & 0x7fffffu;
  // 符号、阶码、尾数三段与图中的 binary32 布局对应。任一段不符，图就不成立。
  if (bits != 0x3f800000u) {
    syslab_step("1.0f 的内存位型不是 0x3f800000。");
    return syslab_fail("ieee754-single");
  }
  syslab_note("LANG", "C++20 requires two's complement for signed integers");
  syslab_note("LANG", "1.0f bits are checked only because numeric_limits reports IEC 559");
  syslab_step("int -1 的每个字节都是 ff，与图中的四个字节一致。");
  syslab_step("numeric_limits<float>::is_iec559 为真，继续读取 1.0f 的内存。");
  syslab_step("1.0f 的符号位是 " + std::to_string(sign) + "，阶码是 " + std::to_string(exponent) + "，尾数是 " +
              std::to_string(fraction) + "，与图中的 binary32 布局一致。");
  return syslab_pass();
}
