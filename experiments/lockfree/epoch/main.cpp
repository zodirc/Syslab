#include "experiments/common/check.hpp"
#include "syslab/atomic/epoch.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
// 应用：读者仍在使用对象时，回收逻辑已经删除该对象。
// 原理：线程处于 pin 期间退役一个对象，删除计数为 0。unpin 后再退役两个对象，删除计数为 2。最后一个对象可以留到下一次宽限期。
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
  syslab_note("OBSERVED", "the last retired object can remain until a later grace period; two of the three are deleted here");
    syslab_step("pin 期间退役一个对象后，删除计数为 0。unpin 后再退役两个对象，删除计数为 2。");
  return syslab_pass();
}
