#include "experiments/common/check.hpp"

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

// 应用：一次映射超过了进程的地址空间余量。
// 原理：RLIMIT_AS 的软限制设为当前 VmSize 加上固定余量。余量内的映射可以写入，超过余量的映射返回 MAP_FAILED。
int main() {
  const long page = ::sysconf(_SC_PAGESIZE);
  if (page <= 0) {
    syslab_step("没有得到页长。");
    return syslab_fail("pagesize");
  }
  unsigned long vm_kb = 0;
  FILE* status = std::fopen("/proc/self/status", "r");
  if (status == nullptr) {
    syslab_step("没有读到 VmSize。");
    return syslab_fail("status");
  }
  char line[256];
  while (std::fgets(line, sizeof(line), status) != nullptr) {
    if (std::sscanf(line, "VmSize: %lu", &vm_kb) == 1) {
      break;
    }
  }
  std::fclose(status);
  if (vm_kb == 0) {
    syslab_step("VmSize 是 0。");
    return syslab_fail("vmsize");
  }

  rlimit limit{};
  if (::getrlimit(RLIMIT_AS, &limit) != 0) {
    syslab_step("没有读到 RLIMIT_AS。");
    return syslab_fail("getrlimit");
  }
  const unsigned long long headroom = 64ull << 20;
  const unsigned long long ceiling = static_cast<unsigned long long>(vm_kb) * 1024ull + headroom;
  if (limit.rlim_max != RLIM_INFINITY && static_cast<unsigned long long>(limit.rlim_max) < ceiling) {
    syslab_step("RLIMIT_AS 的硬限制低于要设置的软限制。");
    return syslab_unsupported("rlimit");
  }
  limit.rlim_cur = static_cast<rlim_t>(ceiling);
  if (::setrlimit(RLIMIT_AS, &limit) != 0) {
    syslab_step("RLIMIT_AS 软限制没有改成当前地址空间加上 64MiB。");
    return syslab_unsupported("setrlimit");
  }

  const std::size_t page_size = static_cast<std::size_t>(page);
  void* small = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  void* large = ::mmap(nullptr, static_cast<std::size_t>(headroom + (16ull << 20)), PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  const bool small_ok = small != MAP_FAILED;
  const bool large_rejected = large == MAP_FAILED;
  if (small_ok) {
    static_cast<unsigned char*>(small)[0] = 1;
    ::munmap(small, page_size);
  }
  if (!large_rejected) {
    ::munmap(large, static_cast<std::size_t>(headroom + (16ull << 20)));
  }
  if (!small_ok || !large_rejected) {
    syslab_step("余量内的映射或超过余量的映射与 RLIMIT_AS 不一致。");
    return syslab_fail("ceiling");
  }
  syslab_metric("headroom_bytes", static_cast<long long>(headroom));
  syslab_note("OS", "RLIMIT_AS admits a mapping inside the headroom and rejects a larger one with MAP_FAILED");
  syslab_step("64MiB 余量内的一页映射可以写入。超过余量的映射返回 MAP_FAILED。");
  return syslab_pass();
}
