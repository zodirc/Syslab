#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

// 现象：目录模式 0644 时，目录里的文件读不开。改成 0755 后读到 DATA。
// 原理：穿越目录需要执行位。文件自己的读位足够，也不能代替目录的执行位。
int main() {
  if (::mkdir("box", 0755) != 0) {
    syslab_step("目录没有建立。");
    return syslab_fail("mkdir");
  }
  const int created = ::open("box/item", O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (created < 0 || ::write(created, "DATA", 4) != 4) {
    if (created >= 0) {
      ::close(created);
    }
    syslab_step("目录里的文件没有写好。");
    return syslab_fail("write");
  }
  ::close(created);
  if (::chmod("box", 0644) != 0) {
    syslab_step("没有去掉目录的执行位。");
    return syslab_fail("chmod");
  }
  const int denied = ::open("box/item", O_RDONLY);
  if (denied >= 0) {
    ::close(denied);
    syslab_step("没有执行位的目录仍然能打开里面的文件。");
    return syslab_fail("opened");
  }
  if (errno != EACCES) {
    syslab_step("拒绝穿越目录的错误不是 EACCES。");
    return syslab_fail("errno");
  }
  if (::chmod("box", 0755) != 0) {
    syslab_step("没有把目录改回 0755。");
    return syslab_fail("restore");
  }
  const int opened = ::open("box/item", O_RDONLY);
  char body[4] = {};
  if (opened < 0 || ::read(opened, body, 4) != 4 || std::memcmp(body, "DATA", 4) != 0) {
    if (opened >= 0) {
      ::close(opened);
    }
    syslab_step("恢复执行位后没有读到 DATA。");
    return syslab_fail("read");
  }
  ::close(opened);
  syslab_note("OS", "searching a directory requires the execute bit even when the file itself is readable");
  syslab_step("目录模式 0644 时 open 返回 EACCES。改为 0755 后读到 DATA。");
  return syslab_pass();
}
