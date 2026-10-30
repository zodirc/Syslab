#if defined(__x86_64__)
  .text
  .globl abi_sum6
  .type abi_sum6, @function
abi_sum6:
  movq %rdi, %rax
  addq %rsi, %rax
  addq %rdx, %rax
  addq %rcx, %rax
  addq %r8, %rax
  addq %r9, %rax
  ret
#elif defined(__aarch64__)
  .text
  .globl abi_sum6
  .type abi_sum6, @function
abi_sum6:
  add x0, x0, x1
  add x0, x0, x2
  add x0, x0, x3
  add x0, x0, x4
  add x0, x0, x5
  ret
#else
#error unsupported architecture
#endif
