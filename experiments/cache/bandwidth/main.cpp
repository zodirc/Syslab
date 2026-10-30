#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>
// 应用：复制带宽的数字异常，目的缓冲区的内容尚未与源缓冲区核对。
// 原理：memcpy 复制 8 MiB 之后，目的缓冲区与源缓冲区逐字节相同。单次耗时不是带宽中位数，也没有绑定频率或 NUMA 节点。
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
  syslab_note("OBSERVED", "one copy duration is not a bandwidth conclusion; repeat runs belong in the report metrics");
    syslab_step("8 MiB 复制之后，目的缓冲区与源缓冲区逐字节相同。耗时只作为一次样本记录。");
  return syslab_pass();
}
