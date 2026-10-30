#include "experiments/common/check.hpp"
#include "syslab/atomic/lockfree_stack.hpp"
#include <set>
#include <thread>
#include <vector>
// 应用：无锁栈在并发压入之后丢失元素或弹出重复值。
// 原理：四个线程共压入 800 个互异值。弹出值的集合大小为 800。指针原子不是始终无锁时，结果为不支持。该栈不是 wait-free。
int main() {
  if (!syslab::LockFreeStack<int>{}.is_lock_free()) {
    syslab_step("指针原子不是始终无锁，本节停止。");
    return syslab_unsupported("pointer-atomic-not-lock-free");
  }
  syslab::LockFreeStack<int> stack;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&, index]() {
      for (int item = 0; item < 200; ++item) stack.push(index * 200 + item);
    });
  }
  for (std::thread& thread : threads) thread.join();
  std::set<int> values;
  int item = 0;
  while (stack.pop(item)) values.insert(item);
  if (values.size() != 800) {
    syslab_step("互异值的数量与发送或压入的数量不一致。");
    return syslab_fail("values");
  }
  syslab_note("OBSERVED", "pop retires nodes through hazard pointers; progress is lock-free only while the pointer atomic is always lock-free");
    syslab_step("弹出值的集合大小为 800，与压入的互异值数量相同。");
  return syslab_pass();
}
