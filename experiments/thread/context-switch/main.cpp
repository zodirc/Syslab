#include "experiments/common/check.hpp"
#include <sched.h>
#include <sys/resource.h>
// 应用：用自愿上下文切换次数解释一次 yield 带来的延迟。
// 原理：连续 1000 次 sched_yield 前后，ru_nvcsw 的差值是内核报告的自愿切换样本。差值为 0 仍然是有效样本，不能据此判定切换没有发生或一定发生。
int main() {
  struct rusage before {};
  struct rusage after {};
  if (getrusage(RUSAGE_SELF, &before) != 0) {
    syslab_step("没有读到资源使用计数。");
    return syslab_fail("rusage");
  }
  for (int index = 0; index < 1000; ++index) sched_yield();
  if (getrusage(RUSAGE_SELF, &after) != 0) {
    syslab_step("没有读到资源使用计数。");
    return syslab_fail("rusage");
  }
  const long long delta = static_cast<long long>(after.ru_nvcsw - before.ru_nvcsw);
  syslab_metric("nvcsw_delta", delta);
  syslab_note("OS", "ru_nvcsw counts voluntary switches reported by this kernel; zero is a valid sample");
    syslab_step("1000 次 sched_yield 前后的 ru_nvcsw 差值已经记录。差值为 0 也是一次有效样本。");
  return syslab_pass();
}
