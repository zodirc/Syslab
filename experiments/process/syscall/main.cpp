#include "experiments/common/check.hpp"
#include <unistd.h>
// 应用：日志或性能分析认为某次调用多进了一次内核，但名单与跟踪对不上。
// 原理：getpid 返回当前进程的正数标识。系统调用跟踪记录本次实际出现的入口。跟踪工具不存在时，观察记为不可用，返回值检查仍然有效。
int main() {
  const long pid = static_cast<long>(getpid());
  syslab_metric("pid", pid);
  if (pid <= 0) {
    syslab_step("getpid 没有返回正数。");
    return syslab_fail("pid");
  }
  syslab_note("OS", "getpid is a system call; the trace names the entries actually observed");
    syslab_step("getpid 返回正数。跟踪里实际出现的调用以本次记录为准。");
  return syslab_pass();
}
