# 实验索引

每个实验的检查和边界写在该目录的 `README.md` 与 `expected/behavior.md`。这里只列出标识和目录。

| ID | 目录 |
|---|---|
| OBJ-001 | `experiments/object/copy` |
| OBJ-002 | `experiments/object/move` |
| CPU-001 | `experiments/cpu/integer-float` |
| CPU-002 | `experiments/cpu/call-return` |
| CPU-003 | `experiments/cpu/stack-frame` |
| CPU-004 | `experiments/abi/calling-convention` |
| CPU-005 | `experiments/cpu/virtual-call` |
| CPU-006 | `experiments/cpu/elf-structure` |
| CPU-007 | `experiments/cpu/dynamic-link` |
| CPU-008 | `experiments/compiler/optimization` |
| CPU-009 | `experiments/cpu/atomic-instructions` |
| CPU-010 | `experiments/cpu/branch-pattern` |
| MEM-001 | `experiments/memory/arena` |
| MEM-002 | `experiments/memory/pool` |
| MEM-003 | `experiments/memory/freelist` |
| MEM-004 | `experiments/memory/buddy` |
| MEM-005 | `experiments/memory/size-class` |
| MEM-006 | `experiments/memory/thread-cache` |
| MEM-007 | `experiments/memory/virtual-memory` |
| MEM-008 | `experiments/memory/mprotect` |
| MEM-009 | `experiments/memory/ownership` |
| MEM-010 | `experiments/memory/corruption` |
| OS-001 | `experiments/process/syscall` |
| OS-002 | `experiments/process/fork-cow` |
| OS-003 | `experiments/thread/create` |
| OS-004 | `experiments/thread/address-space` |
| OS-005 | `experiments/thread/wake` |
| OS-006 | `experiments/thread/affinity` |
| OS-007 | `experiments/thread/context-switch` |
| OS-008 | `experiments/process/ipc` |
| OS-009 | `experiments/process/signal` |
| OS-010 | `experiments/kernel/numa` |
| SYNC-001 | `experiments/synchronization/tas` |
| SYNC-002 | `experiments/synchronization/ticket` |
| SYNC-003 | `experiments/synchronization/mcs` |
| SYNC-004 | `experiments/synchronization/mutex` |
| SYNC-005 | `experiments/synchronization/rwlock` |
| SYNC-006 | `experiments/synchronization/recursive` |
| SYNC-007 | `experiments/synchronization/condition` |
| SYNC-008 | `experiments/synchronization/semaphore` |
| SYNC-009 | `experiments/synchronization/barrier` |
| SYNC-010 | `experiments/synchronization/once` |
| AT-001 | `experiments/atomic/data-race` |
| AT-002 | `experiments/atomic/relaxed-count` |
| AT-003 | `experiments/atomic/message-passing` |
| AT-004 | `experiments/atomic/store-buffering` |
| AT-005 | `experiments/atomic/seq-cst` |
| AT-006 | `experiments/atomic/fence` |
| AT-007 | `experiments/atomic/aba` |
| AT-008 | `experiments/atomic/false-sharing` |
| AT-009 | `experiments/lockfree/stack` |
| AT-010 | `experiments/lockfree/spsc` |
| AT-011 | `experiments/lockfree/mpmc` |
| AT-012 | `experiments/lockfree/hazard` |
| AT-013 | `experiments/lockfree/epoch` |
| AT-014 | `experiments/atomic/architecture` |
| CACHE-001 | `experiments/cache/stride` |
| CACHE-002 | `experiments/cache/bandwidth` |
| CACHE-003 | `experiments/cache/false-sharing` |
| RT-001 | `experiments/compiler/object-layout` |
| RT-002 | `experiments/compiler/exception-rtti` |
| CO-001 | `experiments/coroutine/fiber` |
| CO-002 | `experiments/coroutine/task` |
| CO-003 | `experiments/coroutine/work-stealing` |

设计正文中的实验表覆盖 CPU、OS 和 AT。内存、同步、缓存和运行时实验使用同一 manifest 规则，对应这些模块要求实现并被观察的机制。
