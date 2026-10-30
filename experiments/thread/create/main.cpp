#include "experiments/common/check.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <thread>
// 应用：多线程日志混在一起，无法判断一行来自哪个执行流。
// 原理：Linux 为每个任务分配不同的 tid。父线程与新线程的 tid 都为正数，并且互不相等。本节不描述 task_struct 的布局。
int main() {
  const long parent = syscall(SYS_gettid);
  long child = 0;
  std::thread thread([&]() { child = syscall(SYS_gettid); });
  thread.join();
  syslab_metric("parent_tid", parent);
  syslab_metric("child_tid", child);
  if (parent <= 0 || child <= 0 || parent == child) {
    syslab_step("两个线程的 tid 不是互不相等的正数。");
    return syslab_fail("tid");
  }
  syslab_note("OS", "Linux tasks have distinct tids; this does not describe task_struct layout");
    syslab_step("父线程与新线程的 tid 都为正数，并且互不相等。");
  return syslab_pass();
}
