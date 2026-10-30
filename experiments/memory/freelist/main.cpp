#include "experiments/common/check.hpp"
#include "syslab/memory/freelist_allocator.hpp"
// 应用：已释放的总字节数足够，但一次较大的分配仍然失败。
// 原理：空闲链表把相邻空闲区间合并。三块 1000 字节全部释放后，一次 2500 字节的请求能够成功。不相邻的空闲区间不会被当成一整块。
int main() {
  syslab::FreeListAllocator allocator(64 * 1024);
  void* a = allocator.allocate({1000, 16, 0});
  void* b = allocator.allocate({1000, 16, 0});
  void* c = allocator.allocate({1000, 16, 0});
  if (a == nullptr || b == nullptr || c == nullptr) {
    syslab_step("分配返回空指针。");
    return syslab_fail("alloc");
  }
  allocator.deallocate(a, 1000, 16);
  allocator.deallocate(b, 1000, 16);
  allocator.deallocate(c, 1000, 16);
  void* large = allocator.allocate({2500, 16, 0});
  if (large == nullptr) {
    syslab_step("相邻空闲块释放后，较大的请求仍然失败。");
    return syslab_fail("coalesce");
  }
  allocator.deallocate(large, 2500, 16);
  syslab_note("OBSERVED", "adjacent free spans are merged by this free-list implementation");
    syslab_step("三块相邻的 1000 字节释放之后，2500 字节的请求分配成功。");
  return syslab_pass();
}
