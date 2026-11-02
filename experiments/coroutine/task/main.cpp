#include "experiments/common/check.hpp"
#include "syslab/runtime/coroutine.hpp"
#include <stdexcept>
#include <string>
syslab::Task sequence(syslab::CoroutineScheduler& scheduler, std::string& log, const char* first, const char* second) {
  log += first;
  co_await syslab::yield_to(scheduler);
  log += second;
}
// 应用：协程中的异常没有到达调用 run 的一方，或者两个任务的输出顺序与挂起点不符。
// 原理：两个协程在挂起点交替，日志为 ACBD。任务抛出 runtime_error 时，run 重新抛出该异常。协程在调度器恢复之前不运行，也不自动并行。
int main() {
  std::string log;
  {
    syslab::CoroutineScheduler scheduler;
    scheduler.spawn(sequence(scheduler, log, "A", "B"));
    scheduler.spawn(sequence(scheduler, log, "C", "D"));
    scheduler.run();
  }
  if (log != "ACBD") {
    syslab_step("执行顺序或交出顺序与预期不一致。");
    return syslab_fail("order");
  }
  syslab::CoroutineScheduler failing;
  auto boom = []() -> syslab::Task {
    throw std::runtime_error("boom");
    co_return;
  };
  failing.spawn(boom());
  bool threw = false;
  try {
    failing.run();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!threw) {
    syslab_step("run 没有重新抛出任务中的异常。");
    return syslab_fail("exception");
  }
  syslab_note("LANG", "a coroutine task is not a thread and does not run until the scheduler resumes it");
    syslab_step("两个协程在挂起点交替，日志为 ACBD。run 重新抛出任务中的 runtime_error。");
  return syslab_pass();
}
