#include "experiments/common/check.hpp"
#include <atomic>
#include <cstdint>
// 应用：无锁结构里，节点被释放后又分配到同一地址，比较交换把新节点误认成旧节点。
// 原理：本节用一个 64 位字的高 32 位做版本号。第一次比较交换把版本从 0 改为 1。随后仍用版本 0 的期望值做比较交换，必须失败，低 32 位保持为 1。这是版本号使旧期望失效的演示。
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
  syslab_note("OBSERVED", "the high half is a tag; a stale low-half expectation fails after the tag changes");
    syslab_step("版本号从 0 改为 1 之后，仍携带版本 0 的比较交换失败，字的低 32 位保持为 1。");
  return syslab_pass();
}
