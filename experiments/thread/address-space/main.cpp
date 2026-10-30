#include "experiments/common/check.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
// 应用：一次写入只在一部分执行流里可见，需要判断那些执行流是线程还是进程。
// 原理：同一进程的线程共享地址空间，新线程写入的 1 在 join 之后对原线程可见。fork 出的子进程写入 5，不改变父进程中的 1。
int main() {
  int shared = 0;
  std::thread thread([&]() { shared = 1; });
  thread.join();
  if (shared != 1) {
    syslab_step("新线程的写入在 join 之后不可见。");
    return syslab_fail("thread");
  }
  int isolated = 1;
  const pid_t child = fork();
  if (child < 0) {
    syslab_step("fork 失败。");
    return syslab_fail("fork");
  }
  if (child == 0) {
    isolated = 5;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
    syslab_step("没有等到子进程结束。");
    return syslab_fail("wait");
  }
  if (isolated != 1) {
    syslab_step("子进程的写入改变了父进程中的值。");
    return syslab_fail("fork-visibility");
  }
  syslab_note("OS", "threads share the process address space; the child after fork does not publish stores back to the parent");
    syslab_step("新线程写入的 1 在 join 之后可见。子进程写入的 5 不改变父进程中的值。");
  return syslab_pass();
}
