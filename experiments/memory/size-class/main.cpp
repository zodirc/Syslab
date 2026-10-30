#include "experiments/common/check.hpp"
#include "syslab/memory/size_class_allocator.hpp"
// 应用：小对象和大对象释放之后，分配器统计仍然显示有字节未归还。
// 原理：不超过大小类的请求进入大小类。超过最大类的请求使用单独映射。两条路径都释放后，allocated_bytes 为 0。
int main() {
  syslab::SizeClassAllocator allocator(32);
  void* small = allocator.allocate({24, 8, 0});
  void* large = allocator.allocate({8192, 16, 0});
  if (small == nullptr || large == nullptr) {
    syslab_step("分配返回空指针。");
    return syslab_fail("alloc");
  }
  allocator.deallocate(small, 24, 8);
  allocator.deallocate(large, 8192, 16);
  const syslab::AllocationStats stats = allocator.statistics();
  if (!stats.has(syslab::StatField::Allocated) || stats.allocated_bytes != 0) {
    syslab_step("释放之后已分配字节不是 0。");
    return syslab_fail("stats");
  }
  syslab_note("OBSERVED", "requests above the largest class use a dedicated mapping in this implementation");
    syslab_step("24 字节请求和 8192 字节请求都释放之后，已分配字节为 0。");
  return syslab_pass();
}
