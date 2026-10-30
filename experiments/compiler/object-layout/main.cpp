#include "experiments/common/check.hpp"
#include "syslab/runtime/layout.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
struct Sample { std::uint32_t first; std::uint8_t second; std::uint64_t third; };
struct Base { virtual int id() const { return 1; } virtual ~Base() = default; };
// 应用：把结构体按字节写入文件或网络后，对端字段错位；或者把含虚函数的对象当成普通内存块复制。
// 原理：标准布局类型 Sample 的首字段偏移为 0。含虚函数的 Base 是多态类型，id() 返回 1。非标准布局的字段偏移和虚表位置不是语言保证。
int main() {
  static_assert(std::is_standard_layout<Sample>::value, "Sample is standard layout");
  if (offsetof(Sample, first) != 0) {
    syslab_step("标准布局对象的首字段偏移不是 0。");
    return syslab_fail("offset");
  }
  Base object;
  if (!std::is_polymorphic<Base>::value || object.id() != 1) {
    syslab_step("含虚函数的类型没有表现为多态。");
    return syslab_fail("polymorphic");
  }
  syslab::write_layout<Sample>(std::cout, "Sample",
      {SYSLAB_FIELD(Sample, first), SYSLAB_FIELD(Sample, second), SYSLAB_FIELD(Sample, third)});
  syslab_note("ABI", "field offsets below are for this compiler ABI because Sample is standard layout");
  syslab_note("ABI", "the virtual table layout of Base is not a C++ standard layout");
    syslab_step("Sample::first 的偏移为 0。Base 是多态类型，id() 返回 1。");
  return syslab_pass();
}
