#include "experiments/common/check.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
struct Sample { std::uint32_t first; std::uint8_t second; std::uint64_t third; };
// 原理：Sample 是标准布局。offsetof(Sample, first) 为 0。对象起始地址读出的 4 字节等于 first。
int main() {
  static_assert(std::is_standard_layout<Sample>::value, "Sample is standard layout");
  Sample sample{};
  sample.first = 0xA1B2C3D4u;
  std::uint32_t loaded = 0;
  std::memcpy(&loaded, &sample, sizeof(loaded));
  if (offsetof(Sample, first) != 0 || loaded != sample.first) {
    syslab_step("标准布局对象的首字段不在偏移 0，或起始地址读出的值与 first 不同。");
    return syslab_fail("offset");
  }
  syslab_metric("first", loaded);
  syslab_note("ABI", "Sample is standard layout; the first member is at offset 0 and those bytes are read from the object address");
  syslab_step("Sample::first 的偏移为 0。对象起始地址读出的 4 字节等于 first。");
  return syslab_pass();
}
