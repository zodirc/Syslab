#include "experiments/common/check.hpp"
#include "syslab/concurrency/scheduler.hpp"
#include <string>
// 应用：协作式任务卡住，后续任务没有机会运行。
// 原理：两个 fiber 在 yield 处交替，日志为 A1B1A2B2。不调用 yield 的 fiber 不会被抢占。这是 ucontext 切换，不是内核线程。
int main() {
  syslab::CooperativeScheduler scheduler;
  std::string log;
  scheduler.spawn([&]() {
    log += "A1";
    syslab::CooperativeScheduler::yield();
    log += "A2";
  });
  scheduler.spawn([&]() {
    log += "B1";
    syslab::CooperativeScheduler::yield();
    log += "B2";
  });
  scheduler.run();
  if (log != "A1B1A2B2") {
    syslab_step("执行顺序或交出顺序与预期不一致。");
    return syslab_fail("order");
  }
  syslab_note("OBSERVED", "ucontext switching is cooperative and is not a kernel context switch");
    syslab_step("两个 fiber 在 yield 处交替，日志为 A1B1A2B2。");
  return syslab_pass();
}
