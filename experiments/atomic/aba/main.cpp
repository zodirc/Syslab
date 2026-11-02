#include "experiments/common/check.hpp"
#include <atomic>
#include <cstdint>
// 原理：字初值为 1。第一次比较交换写成 (1<<32)|1。期望值仍为 1 的比较交换失败，低 32 位保持为 1。
int main() {
  std::atomic<std::uint64_t> word{1};
  std::uint64_t stale = word.load();
  const std::uint64_t updated = (1ull << 32) | 1ull;
  if (!word.compare_exchange_strong(stale, updated)) {
    syslab_step("第一次比较交换没有更新版本号。");
    return syslab_fail("first-cas");
  }
  std::uint64_t expected = 1;
  if (word.compare_exchange_strong(expected, (1ull << 32) | 2ull)) {
    syslab_step("旧版本的比较交换成功了。");
    return syslab_fail("aba-succeeded");
  }
  if (word.load() != updated) {
    syslab_step("失败的比较交换改写了当前的字。");
    return syslab_fail("preserved");
  }
  syslab_note("OBSERVED", "after the high half becomes 1, compare_exchange of 1 fails and the low half stays 1");
    syslab_step("版本号从 0 改为 1 之后，仍携带版本 0 的比较交换失败，字的低 32 位保持为 1。");
  return syslab_pass();
}
