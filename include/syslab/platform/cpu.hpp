#pragma once

#include <cstddef>

namespace syslab {

/**
 * @brief Host instruction-set facts that experiments are allowed to print.
 * 实验可以打印的主机指令集事实。
 *
 * cache_line_bytes is a measured value when sysfs exposes it. Otherwise it is
 * zero and cache_line_known is false. A zero value must not be treated as a
 * cache line size.
 * sysfs 暴露缓存行大小时，cache_line_bytes 是测量值。否则它为 0，且 cache_line_known 为假。零不能当成缓存行大小。
 */
struct HostArchitecture {
  const char* name;
  const char* instruction_pointer;
  const char* stack_pointer;
  const char* integer_return_register;
  const char* syscall_instruction;
  const char* memory_model;
  int pointer_bits;
  std::size_t cache_line_bytes;
  bool cache_line_known;
};

/**
 * @brief Returns facts for the architecture that compiled this binary.
 * 返回编译本二进制所用架构的事实。
 * @return Host architecture description. Pointers are string literals.
 * 主机架构描述。指针指向字符串字面量。
 */
HostArchitecture host_architecture();

/**
 * @brief Emits a compiler barrier.
 * 发出编译器屏障。
 *
 * This constraint applies to compiler reordering of memory accesses around
 * the call. It is not a C++ atomic fence and it is not an ISA barrier.
 * 该约束只限制调用点周围内存访问的编译器重排。它不是 C++ 原子 fence，也不是 ISA 屏障。
 */
inline void compiler_barrier() noexcept {
  asm volatile("" ::: "memory");
}

/**
 * @brief Pauses the current logical processor briefly inside a spin loop.
 * 在自旋循环里短暂暂停当前逻辑处理器。
 *
 * The instruction is a scheduling hint. It does not order memory.
 * 该指令是调度提示。它不给内存排序。
 */
inline void cpu_relax() noexcept {
#if defined(__x86_64__)
  asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
  asm volatile("yield" ::: "memory");
#else
  compiler_barrier();
#endif
}

}  // namespace syslab
