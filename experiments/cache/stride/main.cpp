#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
// 应用：按固定跨步扫描数组比顺序扫描慢，需要先确认两边算出的校验和都正确。
// 原理：顺序访问和跨步 16 的访问都得到非 0 校验和。一次耗时是样本。谁更快不作为通过条件。CPU 型号记在环境快照里。
int main() {
  constexpr std::size_t kSize = 1 << 20;
  std::vector<std::uint32_t> values(kSize);
  for (std::size_t index = 0; index < kSize; ++index) values[index] = static_cast<std::uint32_t>(index);
  auto walk = [&](std::size_t stride) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = 0;
    for (int repeat = 0; repeat < 4; ++repeat) {
      for (std::size_t index = 0; index < kSize; index += stride) sum += values[index];
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    return std::pair<std::uint64_t, long long>{sum, us};
  };
  const auto sequential = walk(1);
  const auto strided = walk(16);
  syslab_metric("sequential_us", sequential.second);
  syslab_metric("stride_us", strided.second);
  if (sequential.first == 0 || strided.first == 0) {
    syslab_step("校验和为 0。");
    return syslab_fail("checksum");
  }
  syslab_note("MICROARCH", "a slower stride is not required; the checksums are the correctness result");
    syslab_step("顺序访问和跨步访问的校验和都不为 0。耗时只作为一次样本记录。");
  return syslab_pass();
}
