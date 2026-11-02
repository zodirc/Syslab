#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>
// 原理：源缓冲填满 0x3c。memcpy 复制 8 MiB 之后，目的缓冲与源缓冲逐字节相同。
int main() {
  constexpr std::size_t kBytes = 8 << 20;
  std::vector<unsigned char> source(kBytes, 0x3c);
  std::vector<unsigned char> destination(kBytes, 0);
  const auto start = std::chrono::steady_clock::now();
  std::memcpy(destination.data(), source.data(), kBytes);
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  syslab_metric("copy_us", us);
  syslab_metric("bytes", static_cast<long long>(kBytes));
  if (std::memcmp(destination.data(), source.data(), kBytes) != 0) {
    syslab_step("目的缓冲区与源缓冲区不一致。");
    return syslab_fail("copy");
  }
  syslab_note("OBSERVED", "the destination matched the source after memcpy of 8 MiB");
    syslab_step("8 MiB 复制之后，目的缓冲区与源缓冲区逐字节相同。耗时只作为一次样本记录。");
  return syslab_pass();
}
