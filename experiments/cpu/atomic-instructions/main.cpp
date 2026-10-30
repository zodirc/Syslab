#include "experiments/common/check.hpp"
#include <atomic>
// 应用：计数偶发少一段，怀疑并发的加减被拆成了读、改、写。
// 原理：同一线程上，atomic 的 store 与 fetch_add 按程序顺序可见。本节先写入 1，再写入 2，再加 3。fetch_add 返回的旧值是 2，最终值是 5。具体指令是 lock、xchg 还是 dmb，由反汇编记录。
int main() {
  std::atomic<int> value{0};
  value.store(1, std::memory_order_relaxed);
  value.store(2, std::memory_order_seq_cst);
  const int added = value.fetch_add(3, std::memory_order_seq_cst);
  if (added != 2 || value.load(std::memory_order_seq_cst) != 5) {
    syslab_step("原子读改写的旧值或最终值与程序顺序不一致。");
    return syslab_fail("atomic-result");
  }
  syslab_metric("value", value.load(std::memory_order_relaxed));
  syslab_note("LANG", "the atomic modifications are visible to this thread in program order");
  syslab_note("ISA", "whether seq_cst became xchg, lock, or dmb is an instruction selection observed in the disassembly");
    syslab_step("先存入 2，再加 3。fetch_add 返回 2，最终值为 5。读改写的数值在本线程上保持连续。");
  return syslab_pass();
}
