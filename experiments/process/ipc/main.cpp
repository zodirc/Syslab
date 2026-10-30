#include "experiments/common/check.hpp"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
// 应用：进程之间的数据没有到达，需要区分字节流和共享内存。
// 原理：管道写入两个字节 ok 后，读端得到同样的字节。MAP_SHARED 匿名映射上，子进程写入 42，父进程读到 42。
int main() {
  int fds[2];
  if (pipe(fds) != 0) {
    syslab_step("管道没有建立。");
    return syslab_fail("pipe");
  }
  if (write(fds[1], "ok", 2) != 2) {
    syslab_step("管道写入没有完成。");
    return syslab_fail("write");
  }
  char buffer[2] = {};
  if (read(fds[0], buffer, 2) != 2 || std::memcmp(buffer, "ok", 2) != 0) {
    syslab_step("管道没有读回写入的字节。");
    return syslab_fail("read");
  }
  close(fds[0]);
  close(fds[1]);
  const long page = sysconf(_SC_PAGESIZE);
  void* mapping = mmap(nullptr, static_cast<std::size_t>(page), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
    *static_cast<int*>(mapping) = 42;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
    syslab_step("没有等到子进程结束。");
    return syslab_fail("wait");
  }
  const int published = *static_cast<int*>(mapping);
  munmap(mapping, static_cast<std::size_t>(page));
  if (published != 42) {
    syslab_step("父进程没有读到 MAP_SHARED 映射中的写入。");
    return syslab_fail("shared");
  }
  syslab_note("OS", "MAP_SHARED publishes the store to the parent; MAP_PRIVATE would not be asserted here");
    syslab_step("管道读回写入的两个字节。MAP_SHARED 映射上，父进程读到子进程写入的 42。");
  return syslab_pass();
}
