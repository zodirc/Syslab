#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
// 应用：含分支的循环变慢，需要确认两边的计算结果仍然正确。
// 原理：语言只规定两种循环各自算出确定的校验和。单次耗时是一个样本，不作为哪种分支模式更快的判定。
int main() {
  constexpr int kSize = 200000;
  std::vector<int> values(kSize);
  for (int index = 0; index < kSize; ++index) values[static_cast<std::size_t>(index)] = index;
  auto accumulate = [&](bool mixed) {
    std::uint64_t sum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int repeat = 0; repeat < 20; ++repeat) {
      for (int index = 0; index < kSize; ++index) {
        const int value = values[static_cast<std::size_t>(index)];
        const bool taken = mixed ? ((value * 17) & 64) != 0 : value >= 0;
        if (taken) sum += static_cast<std::uint64_t>(value);
      }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return std::pair<std::uint64_t, long long>{sum, std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()};
  };
  const auto predictable = accumulate(false);
  const auto mixed = accumulate(true);
  syslab_metric("predictable_us", predictable.second);
  syslab_metric("mixed_us", mixed.second);
  if (predictable.first == 0 || mixed.first == 0) {
    syslab_step("校验和为 0。");
    return syslab_fail("checksum");
  }
  syslab_note("LANG", "both loops compute a defined sum");
  syslab_note("MICROARCH", "a shorter predictable loop is not required; branch-predictor behavior is not a language rule");
  syslab_note("OBSERVED", "hardware performance counters were not required for the pass condition");
    syslab_step("固定走向的循环和混合走向的循环都得到非 0 校验和。耗时只作为一次样本记录。");
  return syslab_pass();
}
