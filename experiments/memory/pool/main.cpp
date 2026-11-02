#include "experiments/common/check.hpp"
#include "syslab/memory/pool_allocator.hpp"
// 原理：PoolAllocator(32, 4, 16) 的四次 32 字节请求返回非空。第 5 次 32 字节请求和 64 字节请求返回空指针。归还一块后，32 字节请求返回非空。
int main() {
  syslab::PoolAllocator pool(32, 4, 16);
  void* blocks[4];
  for (int index = 0; index < 4; ++index) {
    blocks[index] = pool.allocate({32, 16, 0});
    if (blocks[index] == nullptr) {
    syslab_step("分配返回空指针。");
    return syslab_fail("alloc");
  }
  }
  if (pool.allocate({32, 16, 0}) != nullptr) {
    syslab_step("池耗尽后仍然分配成功。");
    return syslab_fail("capacity");
  }
  if (pool.allocate({64, 16, 0}) != nullptr) {
    syslab_step("大于块大小的请求被放进了池。");
    return syslab_fail("oversize");
  }
  pool.deallocate(blocks[1], 32, 16);
  if (pool.allocate({32, 16, 0}) == nullptr) {
    syslab_step("释放后的下一次分配没有按预期完成。");
    return syslab_fail("reuse");
  }
  syslab_note("OBSERVED", "four 32-byte requests returned non-null, the fifth and a 64-byte request returned null, and a request after one deallocate returned non-null");
    syslab_step("4 个 32 字节块分配成功。第 5 次和 64 字节请求都失败。归还一块后，32 字节请求再次成功。");
  return syslab_pass();
}
