#include "experiments/common/check.hpp"
#include "syslab/core/error.hpp"
#include "syslab/memory/arena_allocator.hpp"
// 应用：分配器在空请求、空指针释放或同一块的第二次释放上崩溃，或者统计因此漂移。
// 原理：0 字节请求抛出错误。空指针释放是空操作。同一块的第二次释放抛出错误。
int main() {
  syslab::ArenaAllocator arena(64 * 1024);
  bool zero = false;
  try {
    arena.allocate({0, 16, 0});
  } catch (const syslab::Error&) {
    zero = true;
  }
  if (!zero) {
    syslab_step("0 字节请求没有抛出错误。");
    return syslab_fail("zero-size");
  }
  void* pointer = arena.allocate({32, 16, 0});
  arena.deallocate(nullptr, 32, 16);
  arena.deallocate(pointer, 32, 16);
  bool repeated = false;
  try {
    arena.deallocate(pointer, 32, 16);
  } catch (const syslab::Error&) {
    repeated = true;
  }
  if (!repeated) {
    syslab_step("第二次释放同一块没有抛出错误。");
    return syslab_fail("double-free");
  }
  syslab_note("OBSERVED", "null deallocate is a no-op; double free and a zero size are errors");
    syslab_step("0 字节请求抛出错误。空指针释放没有抛出。同一块的第二次释放抛出错误。");
  return syslab_pass();
}
