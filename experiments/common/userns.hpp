#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>

/**
 * @brief Creates a user namespace and maps the caller to uid 0 and gid 0 inside it.
 * 建立用户命名空间，并把调用者映射为其中的 uid 0 和 gid 0。
 *
 * @return 0 when both id maps are written. -1 when unshare or a map write fails.
 * 两份标识映射都写入时返回 0。unshare 或映射写入失败时返回 -1。
 *
 * @note A failed write can leave the process inside the new user namespace.
 * 写入失败时，进程可能已经留在新的用户命名空间中。
 */
inline int syslab_enter_user_namespace() {
  const uid_t user = ::geteuid();
  const gid_t group = ::getegid();
  if (::unshare(CLONE_NEWUSER) != 0) {
    return -1;
  }
  const int setgroups = ::open("/proc/self/setgroups", O_WRONLY);
  if (setgroups < 0 || ::write(setgroups, "deny", 4) != 4) {
    if (setgroups >= 0) {
      ::close(setgroups);
    }
    return -1;
  }
  ::close(setgroups);

  char text[64];
  const int uid_length = std::snprintf(text, sizeof(text), "0 %u 1\n", static_cast<unsigned>(user));
  const int uid_map = ::open("/proc/self/uid_map", O_WRONLY);
  if (uid_map < 0 || uid_length <= 0 ||
      ::write(uid_map, text, static_cast<std::size_t>(uid_length)) != uid_length) {
    if (uid_map >= 0) {
      ::close(uid_map);
    }
    return -1;
  }
  ::close(uid_map);

  const int gid_length = std::snprintf(text, sizeof(text), "0 %u 1\n", static_cast<unsigned>(group));
  const int gid_map = ::open("/proc/self/gid_map", O_WRONLY);
  if (gid_map < 0 || gid_length <= 0 ||
      ::write(gid_map, text, static_cast<std::size_t>(gid_length)) != gid_length) {
    if (gid_map >= 0) {
      ::close(gid_map);
    }
    return -1;
  }
  ::close(gid_map);
  return 0;
}
