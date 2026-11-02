#include "experiments/common/check.hpp"
extern "C" long abi_sum6(long, long, long, long, long, long);
// 应用：C++ 调用汇编函数，或另一种语言调用本程序时，前几个整数参数或返回值错位。
// 原理：当前 ABI 把前六个整数参数放进规定的寄存器，返回值放进返回值寄存器。x86-64 把前六个整数放入 rdi、rsi、rdx、rcx、r8、r9，返回值放入 rax。AArch64 把前六个整数放入 x0 至 x5，返回值放入 x0。abi_sum6 从这些寄存器取数并相加，返回值为 21。
int main() {
  const long sum = abi_sum6(1, 2, 3, 4, 5, 6);
  syslab_metric("sum", sum);
  if (sum != 21) {
    syslab_step("汇编函数的返回值不是 21。");
    return syslab_fail("abi-sum");
  }
#if defined(__x86_64__)
  syslab_note("ABI", "System V AMD64 uses rdi rsi rdx rcx r8 r9 for the first six integer arguments and rax for the result");
#elif defined(__aarch64__)
  syslab_note("ABI", "AArch64 PCS uses x0 through x5 for the first six integer arguments and x0 for the result");
#endif
    syslab_step("abi_sum6(1, 2, 3, 4, 5, 6) 的返回值是 21。六个参数与返回值寄存器的对应关系留在反汇编中。");
  return syslab_pass();
}
