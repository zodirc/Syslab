#include "experiments/common/check.hpp"
#include "syslab/concurrency/scheduler.hpp"
#include <string>
// 原理：两个 fiber 依次调用 yield。run 返回后，日志为 A1B1A2B2。
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
  syslab_note("OBSERVED", "the log after run is A1B1A2B2");
    syslab_step("两个 fiber 在 yield 处交替，日志为 A1B1A2B2。");
  return syslab_pass();
}
