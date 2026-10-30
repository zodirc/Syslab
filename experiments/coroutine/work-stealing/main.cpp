#include "experiments/common/check.hpp"
#include "syslab/concurrency/work_stealing.hpp"
#include <atomic>
#include <cstdint>
// 应用：并行求和的结果与串行公式不一致。
// 原理：工作窃取把 0 到 999 分给工作者，总和等于 999 * 1000 / 2。每个工作者使用自己的双端队列。被窃取的下标仍然来自这个区间。
int main() {
  std::atomic<std::uint64_t> sum{0};
  syslab::parallel_for(4, 1000, [&](std::size_t index) { sum.fetch_add(index, std::memory_order_relaxed); });
  if (sum.load() != 999ull * 1000ull / 2ull) {
    syslab_step("求和结果与本节预期不一致。");
    return syslab_fail("sum");
  }
  syslab_note("OBSERVED", "each worker has its own deque; a stolen item is still one index from the range");
    syslab_step("0 到 999 的并行求和等于 999 * 1000 / 2。");
  return syslab_pass();
}
