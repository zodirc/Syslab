#include "experiments/common/check.hpp"
#include "syslab/atomic/epoch.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
// 原理：pin 期间退役一个对象，删除计数为 0。unpin 后再退役两个对象，删除计数为 2。
int main() {
  syslab::EpochDomain epoch;
  epoch.pin();
  epoch.retire(new int(1), reclaim);
  if (deleted.load() != 0) {
    syslab_step("pin 期间退役的对象被删除了。");
    return syslab_fail("deleted-while-pinned");
  }
  epoch.unpin();
  epoch.retire(new int(2), reclaim);
  epoch.retire(new int(3), reclaim);
  if (deleted.load() != 2) {
    syslab_step("unpin 之后的删除计数不是 2。");
    return syslab_fail("grace");
  }
  syslab_note("OBSERVED", "deleted is 0 while pinned and 2 after unpin; one of the three objects remains deferred");
    syslab_step("pin 期间退役一个对象后，删除计数为 0。unpin 后再退役两个对象，删除计数为 2。");
  return syslab_pass();
}
