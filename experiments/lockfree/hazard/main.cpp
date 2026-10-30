#include "experiments/common/check.hpp"
#include "syslab/atomic/hazard_pointer.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
// 应用：无锁结构在读者仍持有某个地址时回收了该地址，随后发生释放后使用。
// 原理：危险指针仍发布该地址时，回收跳过它。保护期间删除计数等于阈值减 1。清除槽后继续退役，最终删除计数等于两倍阈值再加上阈值减 1。阈值是本教学域的参数：每线程槽数乘以最大线程数。
int main() {
  constexpr int threshold = syslab::HazardDomain::kSlots * syslab::HazardDomain::kMaxThreads;
  int* object = new int(0);
  std::atomic<int*> source{object};
  syslab::HazardDomain domain;
  auto guard = domain.acquire();
  int* protected_pointer = guard.protect(0, source);
  guard.retire(protected_pointer, reclaim);
  for (int index = 0; index < threshold - 1; ++index) guard.retire(new int(index + 1), reclaim);
  if (deleted.load() != threshold - 1) {
    syslab_step("地址仍被发布时，受保护对象被回收了。");
    return syslab_fail("reclaimed-while-protected");
  }
  guard.clear(0);
  for (int index = 0; index < threshold; ++index) guard.retire(new int(1000 + index), reclaim);
  for (int index = 0; index < threshold - 1; ++index) guard.retire(new int(2000 + index), reclaim);
  if (deleted.load() != threshold * 2 + (threshold - 1)) {
    syslab_step("清除发布后的删除计数与预期不一致。");
    return syslab_fail("reclaim-after-clear");
  }
  syslab_note("OBSERVED", "scanning starts after the retired list reaches the published slot budget");
    syslab_step("地址仍被危险指针发布时，该对象没有被删除。清除发布之后，后续扫描回收了它。");
  return syslab_pass();
}
