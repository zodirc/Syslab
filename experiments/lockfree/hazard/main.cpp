#include "experiments/common/check.hpp"
#include "syslab/atomic/hazard_pointer.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
// 原理：槽仍公布该地址时，删除计数为阈值减 1。clear(0) 之后，删除计数为两倍阈值再加上阈值减 1。kSlots 为 2，kMaxThreads 为 64，阈值为 128。
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
    syslab_step("地址仍被危险指针发布时，删除计数为 127。clear(0) 之后，删除计数为 383。");
  return syslab_pass();
}
