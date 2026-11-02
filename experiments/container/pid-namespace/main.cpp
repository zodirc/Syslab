#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"
#include "experiments/common/userns.hpp"

#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

// 应用：容器里的 1 号进程和宿主机上的进程号不是同一套编号。
// 原理：用户命名空间把当前用户映射为 0 之后，unshare(CLONE_NEWPID) 创建的第一个子进程在新命名空间里的 pid 是 1。
int main() {
  if (syslab_enter_user_namespace() != 0) {
    syslab_step("用户命名空间没有建立。");
    return syslab_unsupported("userns");
  }
  if (::unshare(CLONE_NEWPID) != 0) {
    syslab_step("unshare(CLONE_NEWPID) 没有成功。");
    return syslab_unsupported("unshare");
  }
  const pid_t child = ::fork();
  if (child < 0) {
    syslab_step("fork 失败。");
    return syslab_fail("fork");
  }
  if (child == 0) {
    _exit(::getpid() == 1 ? 0 : 2);
  }
  int status = 0;
  if (::waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    syslab_step("第一个子进程的 pid 不是 1。");
    return syslab_fail("pid");
  }
  syslab_note("OS", "after a user namespace mapped to 0, the first child of CLONE_NEWPID has pid 1");
  syslab_step("用户命名空间映射为 0 之后，CLONE_NEWPID 的第一个子进程 pid 是 1。");
  return syslab_pass();
}
