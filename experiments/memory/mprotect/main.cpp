#include "experiments/common/check.hpp"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
// 应用：段错误出现后，无法判断是哪一个进程改了保护位，以及信号落在哪一个进程。
// 原理：子进程把映射改为 PROT_NONE 并写入。内核向该子进程投递 SIGSEGV。父进程继续存活，并观察到子进程因 SIGSEGV 终止。
int main() {
  const long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) {
    syslab_step("读不到页大小，本节不能设置保护位。");
    return syslab_unsupported("page-size");
  }
  void* mapping = mmap(nullptr, static_cast<std::size_t>(page), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    syslab_step("映射没有建立。");
    return syslab_fail("mmap");
  }
  const pid_t child = fork();
  if (child < 0) {
    syslab_step("fork 失败。");
    return syslab_fail("fork");
  }
  if (child == 0) {
    if (mprotect(mapping, static_cast<std::size_t>(page), PROT_NONE) != 0) _exit(2);
    *static_cast<volatile char*>(mapping) = 1;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    syslab_step("没有等到子进程结束。");
    return syslab_fail("wait");
  }
  munmap(mapping, static_cast<std::size_t>(page));
  if (!(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV)) {
    syslab_step("子进程没有因 SIGSEGV 终止。");
    return syslab_fail("expected-sigsegv");
  }
  syslab_note("OS", "the fault is delivered to the child; the parent remains alive");
    syslab_step("子进程在 PROT_NONE 的页上写入后因 SIGSEGV 终止。父进程仍在运行并读到该终止状态。");
  return syslab_pass();
}
