#include "experiments/common/check.hpp"
#include "syslab/core/error.hpp"
#include "syslab/memory/arena_allocator.hpp"
// 原理：释放最后一次 32 字节分配后，下一次相同请求返回同一地址。再次释放该地址抛出 syslab::Error。
int main() {
  syslab::ArenaAllocator arena(64 * 1024);
  void* first = arena.allocate({32, 16, 1});
  void* second = arena.allocate({32, 16, 1});
  if (first == nullptr || second == nullptr) {
    syslab_step("分配返回空指针。");
    return syslab_fail("alloc");
  }
  arena.deallocate(second, 32, 16);
  void* reused = arena.allocate({32, 16, 1});
  if (reused != second) {
    syslab_step("释放尾部之后，下一次相同请求没有返回同一地址。");
    return syslab_fail("lifo-rewind");
  }
  bool threw = false;
  try {
    arena.deallocate(reused, 32, 16);
    arena.deallocate(reused, 32, 16);
  } catch (const syslab::Error&) {
    threw = true;
  }
  if (!threw) {
    syslab_step("第二次释放同一块没有抛出错误。");
    return syslab_fail("double-free");
  }
  syslab_note("OBSERVED", "the next 32-byte request returned the same address, and the second deallocate threw");
    syslab_step("释放最后一次 32 字节分配后，下一次相同请求返回同一地址。第二次释放同一块抛出错误。");
  return syslab_pass();
}
