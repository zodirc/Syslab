#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

// 现象：模式 000 的文件读不开。改成 0644 之后，读到的是当初写入的 SAFE。
// 原理：open 按所有者权限位决定能否读。没有读位时返回 EACCES，不会交出文件内容。
int main() {
  const int created = ::open("secret.bin", O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (created < 0 || ::write(created, "SAFE", 4) != 4) {
    if (created >= 0) {
      ::close(created);
    }
    syslab_step("秘密文件没有写好。");
    return syslab_fail("write");
  }
  ::close(created);
  if (::chmod("secret.bin", 0) != 0) {
    syslab_step("没有把模式改成 000。");
    return syslab_fail("chmod");
  }
  const int denied = ::open("secret.bin", O_RDONLY);
  if (denied >= 0) {
    ::close(denied);
    syslab_step("模式 000 的文件仍然能读。");
    return syslab_fail("opened");
  }
  if (errno != EACCES) {
    syslab_step("拒绝读的错误不是 EACCES。");
    return syslab_fail("errno");
  }
  if (::chmod("secret.bin", 0644) != 0) {
    syslab_step("没有把模式改回 0644。");
    return syslab_fail("restore");
  }
  const int opened = ::open("secret.bin", O_RDONLY);
  char body[4] = {};
  if (opened < 0 || ::read(opened, body, 4) != 4 || std::memcmp(body, "SAFE", 4) != 0) {
    if (opened >= 0) {
      ::close(opened);
    }
    syslab_step("模式 0644 没有读回 SAFE。");
    return syslab_fail("read");
  }
  ::close(opened);
  syslab_note("OS", "open uses the owner read bit; mode 000 returns EACCES and mode 0644 returns the stored bytes");
  syslab_step("模式 000 时 open 返回 EACCES。改为 0644 后读到 SAFE。");
  return syslab_pass();
}
