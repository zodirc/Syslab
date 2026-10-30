#include "experiments/common/check.hpp"
#include "syslab/memory/virtual_memory.hpp"
#include <cstring>
// 应用：mmap 之后读回的字节与写入不一致。
// 原理：匿名私有映射上，写入 128 个字节 0x5a，同一映射的最后一个字节读回 0x5a。本节不判断这些页是否已经驻留物理内存。
int main() {
  syslab::MappedRegion region(8192);
  if (region.data() == nullptr || region.size() < 8192) {
    syslab_step("映射没有建立。");
    return syslab_fail("mmap");
  }
  std::memset(region.data(), 0x5a, 128);
  if (static_cast<unsigned char*>(region.data())[127] != 0x5a) {
    syslab_step("同一映射读回的字节与写入不一致。");
    return syslab_fail("readback");
  }
  syslab_metric("mapped_bytes", static_cast<long long>(region.size()));
  syslab_note("OS", "the region is an anonymous private mapping; residency is not asserted");
    syslab_step("映射写入 0x5a 之后，同一映射的第 128 个字节读回 0x5a。");
  return syslab_pass();
}
