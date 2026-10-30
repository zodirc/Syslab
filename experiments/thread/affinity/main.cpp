#include "experiments/common/check.hpp"
#include <sched.h>
// 应用：线程没有运行在指定的 CPU 上。
// 原理：sched_setaffinity 把掩码设为只含 CPU 0 之后，sched_getaffinity 读回的掩码包含 CPU 0。调度器仍可能推迟该线程实际上 CPU。设置失败时，本节记为当前主机不能完成这项观察。
int main() {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    syslab_step("sched_setaffinity 失败，本节不能读回亲和性掩码。");
    return syslab_unsupported("setaffinity");
  }
  cpu_set_t actual;
  CPU_ZERO(&actual);
  if (sched_getaffinity(0, sizeof(actual), &actual) != 0) {
    syslab_step("没有读回亲和性掩码。");
    return syslab_fail("getaffinity");
  }
  if (!CPU_ISSET(0, &actual)) {
    syslab_step("读回的掩码不包含 CPU 0。");
    return syslab_fail("cpu0");
  }
  syslab_note("OS", "the affinity mask admits CPU 0; the scheduler may still delay running the thread");
    syslab_step("读回的亲和性掩码包含 CPU 0。");
  return syslab_pass();
}
