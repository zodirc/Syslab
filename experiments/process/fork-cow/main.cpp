#include "experiments/common/check.hpp"
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
// 应用：fork 之后，父进程或子进程看到了对方后来写入的变量。
// 原理：子进程在 fork 之后把变量改为 2 并退出。父进程中的同一变量保持为 1。地址空间分离解释这次隔离。缺页次数不能单独证明写时复制。
int main() {
  int value = 1;
  const pid_t child = fork();
  if (child < 0) {
    syslab_step("fork 失败。");
    return syslab_fail("fork");
  }
  if (child == 0) {
    value = 2;
    _exit(value == 2 ? 0 : 1);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    syslab_step("没有等到子进程结束。");
    return syslab_fail("wait");
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    syslab_step("子进程没有正常退出。");
    return syslab_fail("child");
  }
  if (value != 1) {
    syslab_step("子进程的写入改变了父进程中的变量。");
    return syslab_fail("visible-write");
  }
  struct rusage usage {};
  if (getrusage(RUSAGE_CHILDREN, &usage) == 0) syslab_metric("child_minflt", usage.ru_minflt);
  syslab_note("OS", "separate address spaces explain the isolated store");
  syslab_note("OS", "Copy-on-Write is the usual implementation and is not proven by the minflt metric alone");
    syslab_step("子进程把变量写成 2 并正常退出。父进程中的同一变量仍是 fork 之前的 1。");
  return syslab_pass();
}
