"""Remaining SysLab experiments registered into the generator."""


def register_rest(add):
    memory(add)
    operating_system(add)
    synchronization(add)
    atomics(add)
    cache(add)
    runtime(add)


def memory(add):
    add(
        "experiments/memory/arena", "MEM-001", "arena-lifo", "memory.allocator",
        "Rewind an arena tail and detect a double free.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/core/error.hpp"
#include "syslab/memory/arena_allocator.hpp"
int main() {
  syslab::ArenaAllocator arena(64 * 1024);
  void* first = arena.allocate({32, 16, 1});
  void* second = arena.allocate({32, 16, 1});
  if (first == nullptr || second == nullptr) return syslab_fail("alloc");
  arena.deallocate(second, 32, 16);
  void* reused = arena.allocate({32, 16, 1});
  if (reused != second) return syslab_fail("lifo-rewind");
  bool threw = false;
  try {
    arena.deallocate(reused, 32, 16);
    arena.deallocate(reused, 32, 16);
  } catch (const syslab::Error&) {
    threw = true;
  }
  if (!threw) return syslab_fail("double-free");
  syslab_note("OBSERVED", "tail reuse is this arena implementation; it is not a general allocator guarantee");
  return syslab_pass();
}
''',
        "检查线性分配器只按后进先出回退尾部，并拒绝重复释放。",
        "释放最后一次 32 字节分配后，下一次相同请求返回同一地址。随后的第二次释放抛出 syslab::Error。",
        "中间空洞不会被重新分配。该分配器不是线程安全的，也不替换全局 new。",
        ["arena", "double-free"], ["OBSERVED"])

    add(
        "experiments/memory/pool", "MEM-002", "fixed-pool", "memory.allocator",
        "Exhaust a four-block pool and reject an oversized request.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/pool_allocator.hpp"
int main() {
  syslab::PoolAllocator pool(32, 4, 16);
  void* blocks[4];
  for (int index = 0; index < 4; ++index) {
    blocks[index] = pool.allocate({32, 16, 0});
    if (blocks[index] == nullptr) return syslab_fail("alloc");
  }
  if (pool.allocate({32, 16, 0}) != nullptr) return syslab_fail("capacity");
  if (pool.allocate({64, 16, 0}) != nullptr) return syslab_fail("oversize");
  pool.deallocate(blocks[1], 32, 16);
  if (pool.allocate({32, 16, 0}) == nullptr) return syslab_fail("reuse");
  syslab_note("OBSERVED", "free_bytes counts reusable user capacity, not slack inside a live block");
  return syslab_pass();
}
''',
        "检查固定块池的容量、超限请求和释放后的再分配。",
        "4 个块可以分配，第 5 个相同请求返回空。大于块容量的请求返回空。释放其中一个块后，相同请求再次成功。",
        "空指针表示耗尽，不是异常。重复释放会抛出错误，本实验不把重复释放当作通过路径。",
        ["pool"], ["OBSERVED"])

    add(
        "experiments/memory/freelist", "MEM-003", "freelist-coalesce", "memory.allocator",
        "Coalesce adjacent free spans so a larger request succeeds.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/freelist_allocator.hpp"
int main() {
  syslab::FreeListAllocator allocator(64 * 1024);
  void* a = allocator.allocate({1000, 16, 0});
  void* b = allocator.allocate({1000, 16, 0});
  void* c = allocator.allocate({1000, 16, 0});
  if (a == nullptr || b == nullptr || c == nullptr) return syslab_fail("alloc");
  allocator.deallocate(a, 1000, 16);
  allocator.deallocate(b, 1000, 16);
  allocator.deallocate(c, 1000, 16);
  void* large = allocator.allocate({2500, 16, 0});
  if (large == nullptr) return syslab_fail("coalesce");
  allocator.deallocate(large, 2500, 16);
  syslab_note("OBSERVED", "adjacent free spans are merged by this free-list implementation");
  return syslab_pass();
}
''',
        "检查空闲链表在相邻块全部释放后能够满足更大的请求。",
        "三次约 1000 字节的分配全部释放后，2500 字节的请求成功。",
        "合并策略和最小分割余量是这个教学实现的选择。统计字段以 available_fields 为准。",
        ["freelist", "coalesce"], ["OBSERVED"])

    add(
        "experiments/memory/buddy", "MEM-004", "buddy-alloc", "memory.allocator",
        "Allocate and free buddy blocks without overlap.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/buddy_allocator.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
int main() {
  syslab::BuddyAllocator buddy(64 * 1024);
  std::vector<void*> pointers;
  for (int index = 0; index < 8; ++index) {
    void* pointer = buddy.allocate({24, 8, 0});
    if (pointer == nullptr) return syslab_fail("alloc");
    std::memset(pointer, static_cast<int>(0x10 + index), 24);
    pointers.push_back(pointer);
  }
  for (int index = 0; index < 8; ++index) {
    auto* bytes = static_cast<unsigned char*>(pointers[static_cast<std::size_t>(index)]);
    for (int byte = 0; byte < 24; ++byte) {
      if (bytes[byte] != static_cast<unsigned char>(0x10 + index)) return syslab_fail("overlap");
    }
  }
  for (void* pointer : pointers) buddy.deallocate(pointer, 24, 8);
  if (buddy.allocate({128, 16, 0}) == nullptr) return syslab_fail("reuse");
  syslab_note("OBSERVED", "buddy orders are powers of two in this allocator; the mapping size stays the requested power of two");
  return syslab_pass();
}
''',
        "检查伙伴分配器同时存活的块不会互相覆盖，释放后还能满足新请求。",
        "8 个 24 字节块各自保持写入的填充值。全部释放后，128 字节请求成功。",
        "伙伴算法的阶和最小块是实现参数。本实验不把某一阶的空闲链表长度写成可移植结论。",
        ["buddy"], ["OBSERVED"])

    add(
        "experiments/memory/size-class", "MEM-005", "size-class", "memory.allocator",
        "Serve a small request from a class and a large request from a dedicated mapping.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/size_class_allocator.hpp"
int main() {
  syslab::SizeClassAllocator allocator(32);
  void* small = allocator.allocate({24, 8, 0});
  void* large = allocator.allocate({8192, 16, 0});
  if (small == nullptr || large == nullptr) return syslab_fail("alloc");
  allocator.deallocate(small, 24, 8);
  allocator.deallocate(large, 8192, 16);
  const syslab::AllocationStats stats = allocator.statistics();
  if (!stats.has(syslab::StatField::Allocated) || stats.allocated_bytes != 0) return syslab_fail("stats");
  syslab_note("OBSERVED", "requests above the largest class use a dedicated mapping in this implementation");
  return syslab_pass();
}
''',
        "检查大小类分配器对小请求和大请求都能归还，并且已分配字节统计回到 0。",
        "24 字节和 8192 字节的分配都成功，释放后 allocated_bytes 为 0，且该字段被标记为已测量。",
        "大小类边界是实现选择。未置位的统计字段不是 0 这个测量值。",
        ["size-class"], ["OBSERVED"])

    add(
        "experiments/memory/thread-cache", "MEM-006", "thread-cache", "memory.allocator",
        "Allocate and free through per-thread caches.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/size_class_allocator.hpp"
#include "syslab/memory/thread_cache_allocator.hpp"
#include <memory>
#include <thread>
#include <vector>
int main() {
  syslab::ThreadCacheAllocator cache(std::make_unique<syslab::SizeClassAllocator>(128));
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&cache]() {
      void* local[40];
      for (int item = 0; item < 40; ++item) {
        local[item] = cache.allocate({32, 8, 0});
        if (local[item] == nullptr) std::terminate();
      }
      for (void* pointer : local) cache.deallocate(pointer, 32, 8);
    });
  }
  for (std::thread& thread : threads) thread.join();
  const syslab::AllocationStats stats = cache.statistics();
  if (!stats.has(syslab::StatField::Allocated) || stats.allocated_bytes != 0) return syslab_fail("stats");
  syslab_note("OBSERVED", "bins keep an exact request size; a cached 20-byte block is not returned for 32 bytes");
  return syslab_pass();
}
''',
        "检查线程缓存分配器在四个线程分配并释放后，已分配字节回到 0。",
        "每个线程分配并释放 40 个 32 字节块。结束后 allocated_bytes 为 0。",
        "缓存按精确请求大小分箱。跨线程释放回到上游。这不是生产分配器，也不替换全局 new。",
        ["thread-cache"], ["OBSERVED"])

    add(
        "experiments/memory/virtual-memory", "MEM-007", "anonymous-mapping", "memory.virtual",
        "Map an anonymous region, write it, and read it back.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/memory/virtual_memory.hpp"
#include <cstring>
int main() {
  syslab::MappedRegion region(8192);
  if (region.data() == nullptr || region.size() < 8192) return syslab_fail("mmap");
  std::memset(region.data(), 0x5a, 128);
  if (static_cast<unsigned char*>(region.data())[127] != 0x5a) return syslab_fail("readback");
  syslab_metric("mapped_bytes", static_cast<long long>(region.size()));
  syslab_note("OS", "the region is an anonymous private mapping; residency is not asserted");
  return syslab_pass();
}
''',
        "检查匿名映射可以写入并读回。",
        "映射基址非空，映射长度至少 8192，写入的最后一个字节读回为 0x5a。",
        "缺页是否发生、驻留页数量是否增加，需要 mincore 或性能计数器。本实验不把缺页次数当作通过条件。",
        ["mmap"], ["OS"])

    add(
        "experiments/memory/mprotect", "MEM-008", "mprotect-fault", "memory.virtual",
        "A child process faults on a PROT_NONE page.",
        r'''#include "experiments/common/check.hpp"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
int main() {
  const long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) return syslab_unsupported("page-size");
  void* mapping = mmap(nullptr, static_cast<std::size_t>(page), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) return syslab_fail("mmap");
  const pid_t child = fork();
  if (child < 0) return syslab_fail("fork");
  if (child == 0) {
    if (mprotect(mapping, static_cast<std::size_t>(page), PROT_NONE) != 0) _exit(2);
    *static_cast<volatile char*>(mapping) = 1;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) return syslab_fail("wait");
  munmap(mapping, static_cast<std::size_t>(page));
  if (!(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV)) return syslab_fail("expected-sigsegv");
  syslab_note("OS", "the fault is delivered to the child; the parent remains alive");
  return syslab_pass();
}
''',
        "在子进程中把页设为不可访问并写入，确认信号发生在子进程。",
        "子进程因 SIGSEGV 终止。父进程继续并输出通过标记。",
        "信号的内核投递路径不由本实验结果命名。父进程不访问该 PROT_NONE 页。",
        ["mprotect", "sigsegv"], ["OS"])

    add(
        "experiments/memory/ownership", "MEM-009", "shared-weak", "memory.ownership",
        "Break a would-be cycle with a weak pointer and observe destruction.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/runtime/smart_ptr.hpp"
int main() {
  int destroyed = 0;
  struct Node {
    int* counter = nullptr;
    syslab::SharedPtr<Node> next;
    ~Node() { if (counter != nullptr) ++*counter; }
  };
  syslab::WeakPtr<Node> weak;
  {
    auto first = syslab::make_shared<Node>();
    auto second = syslab::make_shared<Node>();
    first->counter = &destroyed;
    second->counter = &destroyed;
    first->next = second;
    weak = syslab::WeakPtr<Node>(second);
    if (!weak.lock()) return syslab_fail("lock");
  }
  if (destroyed != 2 || !weak.expired()) return syslab_fail("lifetime");
  syslab::UniquePtr<int> owned(new int(4));
  syslab::UniquePtr<int> moved(std::move(owned));
  if (owned.get() != nullptr || moved.get() == nullptr || *moved != 4) return syslab_fail("unique");
  syslab_note("LANG", "weak ownership does not keep the object alive");
  return syslab_pass();
}
''',
        "检查 shared/weak 在没有循环强引用时销毁对象，并检查 unique 所有权移动。",
        "两个节点离开作用域后析构次数为 2，weak 过期。移动后的 UniquePtr 持有值 4，原指针为空。",
        "若两边都保存 SharedPtr，对象会留下来。本实验故意不构造那个循环，并在说明里指出这一点。",
        ["shared-ownership", "weak-ownership", "unique-ownership"], ["LANG"])

    add(
        "experiments/memory/corruption", "MEM-010", "allocator-checks", "memory.allocator",
        "Reject a zero size and a repeated free.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/core/error.hpp"
#include "syslab/memory/arena_allocator.hpp"
int main() {
  syslab::ArenaAllocator arena(64 * 1024);
  bool zero = false;
  try {
    arena.allocate({0, 16, 0});
  } catch (const syslab::Error&) {
    zero = true;
  }
  if (!zero) return syslab_fail("zero-size");
  void* pointer = arena.allocate({32, 16, 0});
  arena.deallocate(nullptr, 32, 16);
  arena.deallocate(pointer, 32, 16);
  bool repeated = false;
  try {
    arena.deallocate(pointer, 32, 16);
  } catch (const syslab::Error&) {
    repeated = true;
  }
  if (!repeated) return syslab_fail("double-free");
  syslab_note("OBSERVED", "null deallocate is a no-op; double free and a zero size are errors");
  return syslab_pass();
}
''',
        "检查分配器拒绝 0 字节请求，忽略空释放，并在重复释放时报告错误。",
        "allocate({0}) 抛出 syslab::Error。deallocate(nullptr) 不抛出。对同一块的第二次 deallocate 抛出 syslab::Error。",
        "这些检查属于教学分配器。它们不替代 ASan，也不说明全局 new 有同样行为。",
        ["double-free", "validation"], ["OBSERVED"])


def operating_system(add):
    add(
        "experiments/process/syscall", "OS-001", "getpid-syscall", "os.process",
        "Call getpid and leave the syscall trace as an observation.",
        r'''#include "experiments/common/check.hpp"
#include <unistd.h>
int main() {
  const long pid = static_cast<long>(getpid());
  syslab_metric("pid", pid);
  if (pid <= 0) return syslab_fail("pid");
  syslab_note("OS", "getpid is a system call; the trace names the entries actually observed");
  return syslab_pass();
}
''',
        "确认 getpid 返回正值，并把系统调用跟踪作为观察而不是硬编码名单。",
        "getpid() 大于 0。",
        "跟踪里出现哪些 libc 包装和额外调用，取决于 libcs 和加载器。名单缺失时观察记为 unavailable，实验仍以返回值为准。",
        ["syscall"], ["OS"],
        ["syscall_trace", "stdout", "stderr", "exit_status"])

    add(
        "experiments/process/fork-cow", "OS-002", "fork-address-space", "os.process",
        "Show that a store after fork is not visible to the parent.",
        r'''#include "experiments/common/check.hpp"
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {
  int value = 1;
  const pid_t child = fork();
  if (child < 0) return syslab_fail("fork");
  if (child == 0) {
    value = 2;
    _exit(value == 2 ? 0 : 1);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) return syslab_fail("wait");
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return syslab_fail("child");
  if (value != 1) return syslab_fail("visible-write");
  struct rusage usage {};
  if (getrusage(RUSAGE_CHILDREN, &usage) == 0) syslab_metric("child_minflt", usage.ru_minflt);
  syslab_note("OS", "separate address spaces explain the isolated store");
  syslab_note("OS", "Copy-on-Write is the usual implementation and is not proven by the minflt metric alone");
  return syslab_pass();
}
''',
        "确认 fork 之后子进程的写入父进程看不见，并记录子进程缺页计数但不设阈值。",
        "子进程把自己的 value 改为 2 后正常退出。父进程的 value 仍为 1。",
        "ru_minflt 只是度量。它为 0 或不为 0 都不单独证明物理页是否复制。",
        ["fork", "address-space"], ["OS"])

    add(
        "experiments/thread/create", "OS-003", "thread-id", "os.thread",
        "Show that a new thread has a different tid.",
        r'''#include "experiments/common/check.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <thread>
int main() {
  const long parent = syscall(SYS_gettid);
  long child = 0;
  std::thread thread([&]() { child = syscall(SYS_gettid); });
  thread.join();
  syslab_metric("parent_tid", parent);
  syslab_metric("child_tid", child);
  if (parent <= 0 || child <= 0 || parent == child) return syslab_fail("tid");
  syslab_note("OS", "Linux tasks have distinct tids; this does not describe task_struct layout");
  return syslab_pass();
}
''',
        "确认新线程的 tid 与调用线程不同。",
        "两个 SYS_gettid 的结果都为正，且不相等。",
        "线程创建在内核中的函数名随版本变化，不写入通过条件。strace 只作为观察。",
        ["thread", "tid"], ["OS"],
        ["syscall_trace", "stdout", "stderr", "exit_status"])

    add(
        "experiments/thread/address-space", "OS-004", "thread-vs-fork", "os.thread",
        "A thread sees a store that a forked child does not publish back.",
        r'''#include "experiments/common/check.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
int main() {
  int shared = 0;
  std::thread thread([&]() { shared = 1; });
  thread.join();
  if (shared != 1) return syslab_fail("thread");
  int isolated = 1;
  const pid_t child = fork();
  if (child < 0) return syslab_fail("fork");
  if (child == 0) {
    isolated = 5;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) return syslab_fail("wait");
  if (isolated != 1) return syslab_fail("fork-visibility");
  syslab_note("OS", "threads share the process address space; the child after fork does not publish stores back to the parent");
  return syslab_pass();
}
''',
        "对比线程可见的写入和 fork 之后父进程看不见的写入。",
        "线程把 shared 写成 1，join 之后父线程读到 1。子进程修改 isolated 后，父进程仍读到 1。",
        "fork 之后虚拟地址数值常常相同，这不表示物理页相同。本实验不断言地址数值或 COW。",
        ["address-space"], ["OS"])

    add(
        "experiments/thread/wake", "OS-005", "condition-wake", "os.thread",
        "Wake a thread blocked on a condition variable.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
int main() {
  syslab::Mutex mutex;
  syslab::ConditionVariable condition;
  bool ready = false;
  bool observed = false;
  std::thread waiter([&]() {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    condition.wait(mutex, [&]() { return ready; });
    observed = true;
  });
  {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    ready = true;
    condition.notify_one();
  }
  waiter.join();
  if (!observed) return syslab_fail("wake");
  syslab_note("OS", "the waiter ran after the predicate became true; the kernel wait queue is not identified");
  return syslab_pass();
}
''',
        "确认条件变量等待者在谓词成立后继续执行。",
        "等待线程在 notify 之后把 observed 设为真。",
        "进入内核等待不等于发生了线程切换。本实验不命名调度器函数，也不要求 perf sched。",
        ["condition-variable", "wakeup"], ["OS"])

    add(
        "experiments/thread/affinity", "OS-006", "cpu-affinity", "os.thread",
        "Restrict the process to CPU 0 and read the affinity mask back.",
        r'''#include "experiments/common/check.hpp"
#include <sched.h>
int main() {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) return syslab_unsupported("setaffinity");
  cpu_set_t actual;
  CPU_ZERO(&actual);
  if (sched_getaffinity(0, sizeof(actual), &actual) != 0) return syslab_fail("getaffinity");
  if (!CPU_ISSET(0, &actual)) return syslab_fail("cpu0");
  syslab_note("OS", "the affinity mask admits CPU 0; the scheduler may still delay running the thread");
  return syslab_pass();
}
''',
        "把进程亲和性设为 CPU 0，再读回掩码。",
        "sched_setaffinity 成功，随后的掩码包含 CPU 0。设置失败时结果为 unsupported。",
        "读回掩码不证明下一条指令就在 CPU 0 上退休。本实验不调用 sched_getcpu 作为通过条件。",
        ["affinity"], ["OS"])

    add(
        "experiments/thread/context-switch", "OS-007", "voluntary-switch-metric", "os.thread",
        "Record voluntary context switches around sched_yield without a threshold.",
        r'''#include "experiments/common/check.hpp"
#include <sched.h>
#include <sys/resource.h>
int main() {
  struct rusage before {};
  struct rusage after {};
  if (getrusage(RUSAGE_SELF, &before) != 0) return syslab_fail("rusage");
  for (int index = 0; index < 1000; ++index) sched_yield();
  if (getrusage(RUSAGE_SELF, &after) != 0) return syslab_fail("rusage");
  const long long delta = static_cast<long long>(after.ru_nvcsw - before.ru_nvcsw);
  syslab_metric("nvcsw_delta", delta);
  syslab_note("OS", "ru_nvcsw counts voluntary switches reported by this kernel; zero is a valid sample");
  return syslab_pass();
}
''',
        "在一批 sched_yield 前后读取自愿上下文切换计数，不把增量当作必须为正。",
        "两次 getrusage 都成功。nvcsw 差值写入度量。",
        "增量为 0 仍然通过。该计数不是切换次数的语言定义，也不能代替 perf。",
        ["context-switch"], ["OS", "OBSERVED"],
        ["performance_counter", "stdout", "stderr", "exit_status"])

    add(
        "experiments/process/ipc", "OS-008", "pipe-and-shared-map", "os.ipc",
        "Round-trip two bytes through a pipe and publish a word through MAP_SHARED.",
        r'''#include "experiments/common/check.hpp"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
int main() {
  int fds[2];
  if (pipe(fds) != 0) return syslab_fail("pipe");
  if (write(fds[1], "ok", 2) != 2) return syslab_fail("write");
  char buffer[2] = {};
  if (read(fds[0], buffer, 2) != 2 || std::memcmp(buffer, "ok", 2) != 0) return syslab_fail("read");
  close(fds[0]);
  close(fds[1]);
  const long page = sysconf(_SC_PAGESIZE);
  void* mapping = mmap(nullptr, static_cast<std::size_t>(page), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) return syslab_fail("mmap");
  const pid_t child = fork();
  if (child < 0) return syslab_fail("fork");
  if (child == 0) {
    *static_cast<int*>(mapping) = 42;
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) return syslab_fail("wait");
  const int published = *static_cast<int*>(mapping);
  munmap(mapping, static_cast<std::size_t>(page));
  if (published != 42) return syslab_fail("shared");
  syslab_note("OS", "MAP_SHARED publishes the store to the parent; MAP_PRIVATE would not be asserted here");
  return syslab_pass();
}
''',
        "检查管道往返字节，以及 MAP_SHARED 匿名映射把子进程的写入交给父进程。",
        "管道读回 ok。子进程写入 42 后，父进程从共享映射读到 42。",
        "本实验不比较 pipe、socket 和共享内存的带宽，也不把一次耗时当成性能结论。",
        ["pipe", "shared-memory"], ["OS"])

    add(
        "experiments/process/signal", "OS-009", "signal-delivery", "os.process",
        "Handle one SIGUSR1 delivery.",
        r'''#include "experiments/common/check.hpp"
#include <csignal>
volatile sig_atomic_t delivered = 0;
void handler(int) { delivered = 1; }
int main() {
  struct sigaction action {};
  action.sa_handler = handler;
  if (sigaction(SIGUSR1, &action, nullptr) != 0) return syslab_fail("sigaction");
  if (raise(SIGUSR1) != 0) return syslab_fail("raise");
  if (delivered != 1) return syslab_fail("delivery");
  syslab_note("OS", "the handler ran for SIGUSR1; asynchronous handler restrictions still apply");
  return syslab_pass();
}
''',
        "安装 SIGUSR1 处理函数并确认 raise 之后处理函数执行了一次。",
        "sig_atomic_t 计数变为 1。",
        "处理函数只写 sig_atomic_t。系统调用跟踪是观察，不要求固定的 rt_sigaction 参数。",
        ["signal"], ["OS"],
        ["syscall_trace", "stdout", "stderr", "exit_status"])

    add(
        "experiments/kernel/numa", "OS-010", "numa-nodes", "os.numa",
        "Report unsupported when fewer than two NUMA nodes are visible.",
        r'''#include "experiments/common/check.hpp"
#include <fstream>
#include <string>
int main() {
  int nodes = 0;
  for (int index = 0; index < 64; ++index) {
    const std::string path = "/sys/devices/system/node/node" + std::to_string(index);
    std::ifstream input(path + "/cpulist");
    if (input) ++nodes;
  }
  syslab_metric("numa_nodes", nodes);
  if (nodes < 2) return syslab_unsupported("fewer-than-two-numa-nodes");
  syslab_note("OS", "two node directories are visible; this run does not bind memory and does not assert a latency delta");
  return syslab_pass();
}
''',
        "统计 sysfs 中可见的 NUMA 节点。少于两个节点时明确返回 unsupported。",
        "能读到至少两个 nodeN/cpulist 时通过。否则输出 status=unsupported。",
        "通过只说明拓扑目录可见。本程序不绑定内存，因此不比较节点间访问延迟。",
        ["numa"], ["OS"])


def synchronization(add):
    hammer = r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/{header}"
#include <cstdint>
#include <thread>
#include <vector>
int main() {{
  syslab::{lock} lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {{
    threads.emplace_back([&]() {{
      for (int step = 0; step < 2000; ++step) {{
        syslab::LockGuard<syslab::{lock}> guard(lock);
        ++counter;
      }}
    }});
  }}
  for (std::thread& thread : threads) thread.join();
  if (counter != 8000) return syslab_fail("exclusion");
  syslab_metric("counter", static_cast<long long>(counter));
  syslab_note("OBSERVED", "{note}");
  return syslab_pass();
}}
'''
    specs = [
        ("experiments/synchronization/tas", "SYNC-001", "tas-spinlock", "spinlock.hpp", "SpinLock",
         "test-and-set exclusion; the lock is not fair"),
        ("experiments/synchronization/ticket", "SYNC-002", "ticket-lock", "ticket_lock.hpp", "TicketLock",
         "ticket lock exclusion; handover follows ticket order in this implementation"),
        ("experiments/synchronization/mutex", "SYNC-004", "futex-mutex", "mutex.hpp", "Mutex",
         "futex mutex exclusion; slow-path waits are counted separately by SYNC-010"),
    ]
    for rel, identifier, name, header, lock, note in specs:
        add(rel, identifier, name, "synchronization.lock",
            "Mutual exclusion for " + name + ".",
            hammer.format(header=header, lock=lock, note=note),
            "用四线程递增检查这把锁的互斥。",
            "四个线程各递增 2000 次，最终计数为 8000。",
            note + "。销毁锁时不能仍有线程停在锁内。",
            ["mutual-exclusion"], ["OBSERVED"])

    add(
        "experiments/synchronization/mcs", "SYNC-003", "mcs-lock", "synchronization.lock",
        "Mutual exclusion with an MCS queue lock.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/mcs_lock.hpp"
#include <cstdint>
#include <thread>
#include <vector>
int main() {
  syslab::McsLock lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 1000; ++step) {
        syslab::McsGuard guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 4000) return syslab_fail("exclusion");
  syslab_note("OBSERVED", "each thread waits on its own node; the lock is fair in this implementation");
  return syslab_pass();
}
''',
        "检查 MCS 锁的互斥。",
        "四个线程各持锁递增 1000 次，计数为 4000。",
        "节点必须在 unlock 返回前保持有效。McsGuard 把节点放在守卫内部。",
        ["mcs", "mutual-exclusion"], ["OBSERVED"])

    add(
        "experiments/synchronization/rwlock", "SYNC-005", "rwlock", "synchronization.lock",
        "Exclusive writes are serialized by the writer-preferring rwlock.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/rw_lock.hpp"
#include <thread>
#include <vector>
int main() {
  syslab::RwLock lock;
  int writes = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 3; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 200; ++step) {
        lock.lock_shared();
        lock.unlock_shared();
      }
    });
  }
  threads.emplace_back([&]() {
    for (int step = 0; step < 100; ++step) {
      lock.lock();
      ++writes;
      lock.unlock();
    }
  });
  for (std::thread& thread : threads) thread.join();
  if (writes != 100) return syslab_fail("writes");
  syslab_note("OBSERVED", "this rwlock prefers writers; shared holders exclude the writer");
  return syslab_pass();
}
''',
        "检查写锁互斥，并让共享锁与写锁同时存在。",
        "写者递增 100 次，结果为 100。共享锁路径能够完成。",
        "写优先是这个实现的策略，不是 POSIX rwlock 的唯一合法策略。",
        ["rwlock"], ["OBSERVED"])

    add(
        "experiments/synchronization/recursive", "SYNC-006", "recursive-mutex", "synchronization.lock",
        "The owning thread locks twice, then several threads exclude each other.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/recursive_mutex.hpp"
#include <cstdint>
#include <thread>
#include <vector>
int main() {
  syslab::RecursiveMutex lock;
  lock.lock();
  lock.lock();
  lock.unlock();
  lock.unlock();
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 500; ++step) {
        syslab::LockGuard<syslab::RecursiveMutex> guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 2000) return syslab_fail("exclusion");
  syslab_note("OBSERVED", "reentry is limited to the owning thread");
  return syslab_pass();
}
''',
        "检查同一线程可以重复获取递归互斥量，并且线程之间仍然互斥。",
        "同一线程 lock 两次再 unlock 两次。随后四线程计数为 2000。",
        "非拥有者不能利用递归计数进入临界区。异常路径下守卫析构会释放一次。",
        ["recursive-mutex"], ["OBSERVED"])

    add(
        "experiments/synchronization/condition", "SYNC-007", "condition-variable", "synchronization.lock",
        "A notifier publishes a predicate before waking the waiter.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
int main() {
  syslab::Mutex mutex;
  syslab::ConditionVariable condition;
  int value = 0;
  std::thread worker([&]() {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    condition.wait(mutex, [&]() { return value == 7; });
  });
  {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    value = 7;
    condition.notify_one();
  }
  worker.join();
  if (value != 7) return syslab_fail("predicate");
  syslab_note("OBSERVED", "the sequence counter closes the lost-wakeup window in this condition variable");
  return syslab_pass();
}
''',
        "检查条件变量等待者看到互斥量保护下发布的谓词。",
        "等待线程在 value 变为 7 之后返回。",
        "允许伪唤醒。谓词在持锁时复查。本实验不证明 futex 一定进入了睡眠。",
        ["condition-variable"], ["OBSERVED"])

    add(
        "experiments/synchronization/semaphore", "SYNC-008", "semaphore", "synchronization.lock",
        "At most two threads hold permits.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/semaphore.hpp"
#include <atomic>
#include <thread>
#include <vector>
int main() {
  syslab::Semaphore semaphore(2);
  std::atomic<int> active{0};
  std::atomic<int> maximum{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 40; ++step) {
        semaphore.acquire();
        const int now = active.fetch_add(1) + 1;
        int seen = maximum.load();
        while (now > seen && !maximum.compare_exchange_weak(seen, now)) {}
        active.fetch_sub(1);
        semaphore.release();
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (maximum.load() > 2) return syslab_fail("permits");
  syslab_note("OBSERVED", "the permit count is an atomic; waiters sleep with futex when it is zero");
  return syslab_pass();
}
''',
        "检查计数信号量同时持有的许可不超过初值。",
        "四个线程反复获取两个许可，观测到的最大并发不超过 2。",
        "最大并发小于 2 仍可通过，因为调度可能没有让两个持有者重叠。超过 2 才是失败。",
        ["semaphore"], ["OBSERVED"])

    add(
        "experiments/synchronization/barrier", "SYNC-009", "barrier", "synchronization.lock",
        "Four threads observe a complete generation.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
#include <vector>
int main() {
  syslab::Barrier barrier(4);
  std::atomic<int> arrived{0};
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      arrived.fetch_add(1);
      barrier.arrive_and_wait();
      if (arrived.load() != 4) failures.fetch_add(1);
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (failures.load() != 0) return syslab_fail("generation");
  syslab_note("OBSERVED", "the last arrival publishes the new generation with release");
  return syslab_pass();
}
''',
        "检查四个参与者都到达后，每个人都能看到到达计数为 4。",
        "屏障返回后 arrived 为 4。",
        "到达计数本身用原子发布。屏障等待的是代数，不是忙等。",
        ["barrier"], ["OBSERVED"])

    add(
        "experiments/synchronization/once", "SYNC-010", "mutex-slow-path-and-once", "synchronization.lock",
        "Enter the mutex slow path under contention and run once-initialization once.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/once.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
int main() {
  syslab::Mutex mutex;
  mutex.lock();
  std::atomic<int> started{0};
  std::thread contender([&]() {
    started.store(1);
    mutex.lock();
    mutex.unlock();
  });
  while (started.load() == 0) std::this_thread::yield();
  for (int spin = 0; spin < 100 && mutex.slow_path_count() == 0; ++spin) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto slow = mutex.slow_path_count();
  mutex.unlock();
  contender.join();
  if (slow == 0) return syslab_fail("slow-path");
  syslab::OnceFlag once;
  std::atomic<int> calls{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 8; ++index) {
    threads.emplace_back([&]() { once.call_once([&]() { calls.fetch_add(1); }); });
  }
  for (std::thread& thread : threads) thread.join();
  if (calls.load() != 1) return syslab_fail("once");
  syslab_metric("slow_path", static_cast<long long>(slow));
  syslab_note("OBSERVED", "slow_path_count increments when the fast compare-exchange misses; it does not prove the futex slept");
  return syslab_pass();
}
''',
        "在锁已被持有时让另一个线程进入 Mutex 慢路径，并检查 OnceFlag 只执行一次。",
        "竞争线程启动后，slow_path_count 在解锁前大于 0。八个线程的初始化函数只进入一次。",
        "慢路径计数不是内核等待的证明。OnceFlag 的函数若抛出异常，状态回到未完成，本实验的函数不抛出。",
        ["futex", "once"], ["OBSERVED"], timeout=20000)


def atomics(add):
    add(
        "experiments/atomic/data-race", "AT-001", "synchronized-counter", "atomic.race",
        "Increment a counter with a mutex. The unsynchronized race is not executed.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
#include <vector>
int main() {
  syslab::Mutex mutex;
  int counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 1000; ++step) {
        syslab::LockGuard<syslab::Mutex> guard(mutex);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 4000) return syslab_fail("counter");
  syslab_note("LANG", "an unsynchronized increment of a non-atomic int would be a data race and is not executed");
  return syslab_pass();
}
''',
        "用互斥量保护计数器。不同步的数据竞争不会被运行。",
        "四线程各递增 1000 次，计数为 4000。",
        "数据竞争是未定义行为。本实验不提供会触发竞争的默认路径，因此也不能用一次未崩溃证明没有竞争。",
        ["data-race"], ["LANG"])

    add(
        "experiments/atomic/relaxed-count", "AT-002", "relaxed-fetch-add", "atomic.memory-order",
        "A relaxed fetch_add still produces the exact total.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
#include <vector>
int main() {
  std::atomic<int> counter{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 10000; ++step) counter.fetch_add(1, std::memory_order_relaxed);
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter.load(std::memory_order_relaxed) != 40000) return syslab_fail("total");
  syslab_note("LANG", "relaxed RMW is atomic; it does not establish happens-before with a payload");
  return syslab_pass();
}
''',
        "区分原子性和顺序：relaxed fetch_add 的总和仍然精确。",
        "40000 次 relaxed fetch_add 之后计数为 40000。",
        "本实验不把 relaxed 操作当成发布了其他内存的同步边。",
        ["relaxed", "rmw"], ["LANG"])

    add(
        "experiments/atomic/message-passing", "AT-003", "release-acquire", "atomic.memory-order",
        "An acquire load of a release store observes the payload.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> payload{0};
    std::atomic<int> flag{0};
    std::thread writer([&]() {
      payload.store(42, std::memory_order_relaxed);
      flag.store(1, std::memory_order_release);
    });
    int seen = 0;
    while (flag.load(std::memory_order_acquire) == 0) {}
    seen = payload.load(std::memory_order_relaxed);
    writer.join();
    if (seen != 42) return syslab_fail("payload");
  }
  syslab_note("LANG", "release synchronizes with acquire, so the payload store happens-before the payload load");
  return syslab_pass();
}
''',
        "检查 release 存储与 acquire 加载之间的消息传递。",
        "读者看到 flag 为 1 之后，payload 必定为 42。重复 200 次。",
        "这是语言内存模型的要求，不依赖 x86 TSO。relaxed 标志不能替代这里的 release/acquire。",
        ["release", "acquire"], ["LANG"])

    add(
        "experiments/atomic/store-buffering", "AT-004", "seq-cst-store-buffering", "atomic.memory-order",
        "Sequentially consistent stores must not both observe zero.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
int main() {
  for (int trial = 0; trial < 300; ++trial) {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    syslab::Barrier start(2);
    int r1 = 1;
    int r2 = 1;
    std::thread first([&]() {
      start.arrive_and_wait();
      x.store(1, std::memory_order_seq_cst);
      r1 = y.load(std::memory_order_seq_cst);
    });
    std::thread second([&]() {
      start.arrive_and_wait();
      y.store(1, std::memory_order_seq_cst);
      r2 = x.load(std::memory_order_seq_cst);
    });
    first.join();
    second.join();
    if (r1 == 0 && r2 == 0) return syslab_fail("both-zero");
  }
  syslab_note("LANG", "seq_cst forbids the both-zero store-buffering outcome");
  syslab_note("LANG", "the same shape with relaxed or acq_rel is allowed to observe both zero and is not failed here");
  return syslab_pass();
}
''',
        "检查 seq_cst 存储缓冲实验中不允许两个加载都看到 0。",
        "300 次试验里不出现 r1 与 r2 同时为 0。",
        "relaxed 或 acq_rel 允许两个加载都看到 0。本实验不运行那个允许的结果作为失败条件。没有观察到违规只覆盖这 300 次样本。",
        ["seq-cst", "store-buffering"], ["LANG"])

    add(
        "experiments/atomic/seq-cst", "AT-005", "seq-cst-iriw", "atomic.memory-order",
        "Fail if a seq_cst IRIW sample observes the forbidden outcome.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    syslab::Barrier start(4);
    int r1 = 0, r2 = 0, r3 = 0, r4 = 0;
    std::thread t1([&]() { start.arrive_and_wait(); x.store(1, std::memory_order_seq_cst); });
    std::thread t2([&]() { start.arrive_and_wait(); y.store(1, std::memory_order_seq_cst); });
    std::thread t3([&]() {
      start.arrive_and_wait();
      r1 = x.load(std::memory_order_seq_cst);
      r2 = y.load(std::memory_order_seq_cst);
    });
    std::thread t4([&]() {
      start.arrive_and_wait();
      r3 = y.load(std::memory_order_seq_cst);
      r4 = x.load(std::memory_order_seq_cst);
    });
    t1.join(); t2.join(); t3.join(); t4.join();
    if (r1 == 1 && r2 == 0 && r3 == 1 && r4 == 0) return syslab_fail("iriw");
  }
  syslab_note("LANG", "that IRIW outcome is forbidden for seq_cst");
  syslab_note("OBSERVED", "absence in 200 trials is a sample, not a proof that every execution is ordered");
  return syslab_pass();
}
''',
        "若 seq_cst IRIW 样本出现被禁止的结果，则失败。",
        "不出现 r1==1、r2==0、r3==1、r4==0。",
        "200 次没有出现该结果，只说明这批样本没有违反约束。它不证明硬件从未允许更弱的非 seq_cst 结果。",
        ["seq-cst", "iriw"], ["LANG", "OBSERVED"])

    add(
        "experiments/atomic/fence", "AT-006", "release-acquire-fence", "atomic.memory-order",
        "Pass a payload with relaxed atomics and thread fences.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> payload{0};
    std::atomic<int> flag{0};
    std::thread writer([&]() {
      payload.store(42, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
      flag.store(1, std::memory_order_relaxed);
    });
    while (flag.load(std::memory_order_relaxed) == 0) {}
    std::atomic_thread_fence(std::memory_order_acquire);
    const int seen = payload.load(std::memory_order_relaxed);
    writer.join();
    if (seen != 42) return syslab_fail("fence");
  }
  syslab_note("LANG", "the release fence synchronizes with the acquire fence when the relaxed flag is the synchronizing store/load");
  syslab_note("ISA", "a compiler fence or an ISA barrier is not the same object as atomic_thread_fence");
  return syslab_pass();
}
''',
        "用 relaxed 原子和 atomic_thread_fence 传递 payload。",
        "读者观察到 flag 为 1 并执行 acquire fence 之后，payload 为 42。",
        "编译器屏障、C++ atomic_thread_fence、ISA 屏障和缓存一致性不是同一个机制。这里检查的是 C++ fence。",
        ["fence"], ["LANG", "ISA"])

    add(
        "experiments/atomic/aba", "AT-007", "tagged-aba", "atomic.aba",
        "A tagged pointer makes a stale compare-exchange fail.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <cstdint>
int main() {
  std::atomic<std::uint64_t> word{1};
  std::uint64_t stale = word.load();
  const std::uint64_t updated = (1ull << 32) | 1ull;
  if (!word.compare_exchange_strong(stale, updated)) return syslab_fail("first-cas");
  std::uint64_t expected = 1;
  if (word.compare_exchange_strong(expected, (1ull << 32) | 2ull)) return syslab_fail("aba-succeeded");
  if (word.load() != updated) return syslab_fail("preserved");
  syslab_note("OBSERVED", "the high half is a tag; a stale low-half expectation fails after the tag changes");
  return syslab_pass();
}
''',
        "用带版本号的字模拟 ABA：旧版本的比较交换失败。",
        "第一次 CAS 把 tag 从 0 改为 1。随后用 tag 仍为 0 的期望值做 CAS 必须失败，指针保持为 1。",
        "这不是无锁栈里真实发生的 ABA。它只演示版本号使复用后的旧期望值失效。Slot 必须是可平凡复制的，本结构满足这一点。",
        ["aba", "cas"], ["OBSERVED"])

    add(
        "experiments/atomic/false-sharing", "AT-008", "false-sharing", "atomic.false-sharing",
        "Record adjacent and padded atomic increments. Correctness does not rank their speed.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <chrono>
#include <thread>
int main() {
  struct Adjacent { std::atomic<int> left{0}; std::atomic<int> right{0}; };
  struct Padded { alignas(64) std::atomic<int> left{0}; alignas(64) std::atomic<int> right{0}; };
  auto run = [](auto& pair) {
    const auto start = std::chrono::steady_clock::now();
    std::thread a([&]() { for (int i = 0; i < 200000; ++i) pair.left.fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 200000; ++i) pair.right.fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    return std::pair<int, long long>{pair.left.load() + pair.right.load(), elapsed};
  };
  Adjacent adjacent;
  Padded padded;
  const auto left = run(adjacent);
  const auto right = run(padded);
  syslab_metric("adjacent_us", left.second);
  syslab_metric("padded_us", right.second);
  if (left.first != 400000 || right.first != 400000) return syslab_fail("count");
  syslab_note("MICROARCH", "a slower adjacent pair is common and not required for a pass");
  return syslab_pass();
}
''',
        "分别递增相邻和按缓存行填充的原子，检查计数，并记录耗时。",
        "两种布局的两次递增之和都是 400000。",
        "谁更快取决于微架构和干扰。通过条件不比较两个耗时。",
        ["false-sharing"], ["LANG", "MICROARCH"], risk="resource_intensive")

    add(
        "experiments/lockfree/stack", "AT-009", "lockfree-stack", "atomic.lockfree",
        "Push and pop distinct values through the Treiber stack.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/atomic/lockfree_stack.hpp"
#include <set>
#include <thread>
#include <vector>
int main() {
  if (!syslab::LockFreeStack<int>{}.is_lock_free()) return syslab_unsupported("pointer-atomic-not-lock-free");
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
  if (values.size() != 800) return syslab_fail("values");
  syslab_note("OBSERVED", "pop retires nodes through hazard pointers; progress is lock-free only while the pointer atomic is always lock-free");
  return syslab_pass();
}
''',
        "检查 Treiber 栈在四线程 push 之后能弹出全部 800 个互异值。",
        "弹出值的集合大小为 800。指针原子若不是始终无锁，结果为 unsupported。",
        "该栈不是 wait-free。危险指针保证弹出的节点不会在仍被保护时释放。",
        ["treiber-stack", "hazard-pointer"], ["OBSERVED"])

    add(
        "experiments/lockfree/spsc", "AT-010", "spsc-ring", "atomic.lockfree",
        "One producer and one consumer transfer 10000 integers.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/atomic/spsc_ring.hpp"
#include <thread>
int main() {
  syslab::SpscRing<int, 1024> ring;
  if (!ring.is_lock_free()) return syslab_unsupported("index-atomic-not-lock-free");
  std::thread producer([&]() {
    for (int value = 0; value < 10000; ++value) while (!ring.push(value)) {}
  });
  int expected = 0;
  int value = 0;
  while (expected < 10000) {
    if (ring.pop(value)) {
      if (value != expected) return syslab_fail("order");
      ++expected;
    }
  }
  producer.join();
  syslab_note("OBSERVED", "one producer owns the tail and one consumer owns the head");
  return syslab_pass();
}
''',
        "检查单生产者单消费者环按顺序交出 10000 个整数。",
        "消费者依次得到 0 到 9999。",
        "第二个生产者或第二个消费者不属于这个算法。容量必须是 2 的幂。",
        ["spsc"], ["OBSERVED"])

    add(
        "experiments/lockfree/mpmc", "AT-011", "mpmc-queue", "atomic.lockfree",
        "Four producers and four consumers exchange a set of integers.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/atomic/mpmc_queue.hpp"
#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
int main() {
  syslab::MpmcQueue<int, 1024> queue;
  if (!queue.is_lock_free()) return syslab_unsupported("index-atomic-not-lock-free");
  constexpr int kEach = 500;
  std::atomic<int> popped{0};
  std::mutex mutex;
  std::set<int> values;
  std::vector<std::thread> threads;
  for (int producer = 0; producer < 4; ++producer) {
    threads.emplace_back([&, producer]() {
      for (int item = 0; item < kEach; ++item) while (!queue.push(producer * kEach + item)) {}
    });
  }
  for (int consumer = 0; consumer < 4; ++consumer) {
    threads.emplace_back([&]() {
      while (popped.load() < 4 * kEach) {
        int item = 0;
        if (queue.pop(item)) {
          std::lock_guard<std::mutex> guard(mutex);
          values.insert(item);
          popped.fetch_add(1);
        }
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (values.size() != static_cast<std::size_t>(4 * kEach)) return syslab_fail("values");
  syslab_note("OBSERVED", "a failed push or pop means another thread claimed the position");
  return syslab_pass();
}
''',
        "检查多生产者多消费者队列不丢弃、不重复这批整数。",
        "四个生产者各发送 500 个互异值，消费者集合大小为 2000。",
        "队列有界。T 必须可平凡复制。进度声明依赖索引原子始终无锁。",
        ["mpmc"], ["OBSERVED"])

    add(
        "experiments/lockfree/hazard", "AT-012", "hazard-pointer", "atomic.reclamation",
        "Do not reclaim a pointer that a hazard slot still publishes.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/atomic/hazard_pointer.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
int main() {
  constexpr int threshold = syslab::HazardDomain::kSlots * syslab::HazardDomain::kMaxThreads;
  int* object = new int(0);
  std::atomic<int*> source{object};
  syslab::HazardDomain domain;
  auto guard = domain.acquire();
  int* protected_pointer = guard.protect(0, source);
  guard.retire(protected_pointer, reclaim);
  for (int index = 0; index < threshold - 1; ++index) guard.retire(new int(index + 1), reclaim);
  if (deleted.load() != threshold - 1) return syslab_fail("reclaimed-while-protected");
  guard.clear(0);
  for (int index = 0; index < threshold; ++index) guard.retire(new int(1000 + index), reclaim);
  for (int index = 0; index < threshold - 1; ++index) guard.retire(new int(2000 + index), reclaim);
  if (deleted.load() != threshold * 2 + (threshold - 1)) return syslab_fail("reclaim-after-clear");
  syslab_note("OBSERVED", "scanning starts after the retired list reaches the published slot budget");
  return syslab_pass();
}
''',
        "检查危险指针仍发布某个地址时，该地址不会被回收。",
        "保护期间被删除的对象数等于阈值减 1。清除槽并再次填满退役列表后，删除总数等于阈值的两倍。",
        "扫描阈值是这个教学域的实现参数：每线程槽数乘以最大线程数。它不是标准库接口。",
        ["hazard-pointer"], ["OBSERVED"])

    add(
        "experiments/lockfree/epoch", "AT-013", "epoch-reclamation", "atomic.reclamation",
        "Keep a retired object while the retiring thread is pinned.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/atomic/epoch.hpp"
#include <atomic>
std::atomic<int> deleted{0};
void reclaim(void* pointer) {
  deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}
int main() {
  syslab::EpochDomain epoch;
  epoch.pin();
  epoch.retire(new int(1), reclaim);
  if (deleted.load() != 0) return syslab_fail("deleted-while-pinned");
  epoch.unpin();
  epoch.retire(new int(2), reclaim);
  epoch.retire(new int(3), reclaim);
  if (deleted.load() != 2) return syslab_fail("grace");
  syslab_note("OBSERVED", "the last retired object can remain until a later grace period; two of the three are deleted here");
  return syslab_pass();
}
''',
        "检查线程处于 pin 期间，它刚退役的对象不会被删除。",
        "pin 期间退役一个对象后删除计数为 0。unpin 后再退役两个对象，删除计数为 2。",
        "最后一个对象可以留到下一次宽限期。本域每个线程索引按域记录，一个线程同时 pin 两个域会覆盖索引，实验不那样使用。",
        ["epoch"], ["OBSERVED"])

    add(
        "experiments/atomic/architecture", "AT-014", "one-architecture", "atomic.architecture",
        "Record the architecture this binary was built for.",
        r'''#include "experiments/common/check.hpp"
int main() {
#if defined(__x86_64__)
  syslab_note("ISA", "this binary targets x86-64 TSO; it does not execute AArch64");
  syslab_metric("arch", 1);
#elif defined(__aarch64__)
  syslab_note("ISA", "this binary targets AArch64; it does not execute x86-64");
  syslab_metric("arch", 2);
#else
  return syslab_unsupported("architecture");
#endif
  syslab_note("LANG", "seq_cst constraints in AT-004 do not become weaker on either architecture");
  return syslab_pass();
}
''',
        "标明这一个二进制只覆盖当前 ISA。",
        "x86-64 或 AArch64 宏恰好有一个成立。其他架构返回 unsupported。",
        "一次运行不能对照另一套指令集。允许行为的差异要在对应架构的构建上分别记录。",
        ["tso", "aarch64"], ["ISA", "LANG"],
        ["disassembly", "stdout", "stderr", "exit_status"])


def cache(add):
    add(
        "experiments/cache/stride", "CACHE-001", "stride-checksum", "cache.locality",
        "Checksum sequential and strided walks and record both durations.",
        r'''#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
int main() {
  constexpr std::size_t kSize = 1 << 20;
  std::vector<std::uint32_t> values(kSize);
  for (std::size_t index = 0; index < kSize; ++index) values[index] = static_cast<std::uint32_t>(index);
  auto walk = [&](std::size_t stride) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = 0;
    for (int repeat = 0; repeat < 4; ++repeat) {
      for (std::size_t index = 0; index < kSize; index += stride) sum += values[index];
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    return std::pair<std::uint64_t, long long>{sum, us};
  };
  const auto sequential = walk(1);
  const auto strided = walk(16);
  syslab_metric("sequential_us", sequential.second);
  syslab_metric("stride_us", strided.second);
  if (sequential.first == 0 || strided.first == 0) return syslab_fail("checksum");
  syslab_note("MICROARCH", "a slower stride is not required; the checksums are the correctness result");
  return syslab_pass();
}
''',
        "顺序访问和跨步访问都计算确定的校验和，并记录耗时。",
        "两种求和结果都不为 0。",
        "缓存命中和谁更快不是通过条件。CPU 型号在环境快照里，不从耗时反推。",
        ["cache-line", "stride"], ["LANG", "MICROARCH"], risk="resource_intensive", timeout=60000)

    add(
        "experiments/cache/bandwidth", "CACHE-002", "copy-bandwidth", "cache.bandwidth",
        "Copy a buffer and check the bytes.",
        r'''#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>
int main() {
  constexpr std::size_t kBytes = 8 << 20;
  std::vector<unsigned char> source(kBytes, 0x3c);
  std::vector<unsigned char> destination(kBytes, 0);
  const auto start = std::chrono::steady_clock::now();
  std::memcpy(destination.data(), source.data(), kBytes);
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  syslab_metric("copy_us", us);
  syslab_metric("bytes", static_cast<long long>(kBytes));
  if (std::memcmp(destination.data(), source.data(), kBytes) != 0) return syslab_fail("copy");
  syslab_note("OBSERVED", "one copy duration is not a bandwidth conclusion; repeat runs belong in the report metrics");
  return syslab_pass();
}
''',
        "复制一块缓冲区并逐字节核对。耗时只作为一次样本。",
        "目标缓冲区与源缓冲区的 8 MiB 内容一致。",
        "单次 memcpy 耗时不是带宽中位数，也没有固定频率或 NUMA 绑定。报告不应把它写成硬件上限。",
        ["bandwidth"], ["OBSERVED"], risk="resource_intensive")

    add(
        "experiments/cache/false-sharing", "CACHE-003", "padded-atomics", "cache.false-sharing",
        "Check padded and adjacent atomic totals and record durations.",
        r'''#include "experiments/common/check.hpp"
#include <atomic>
#include <chrono>
#include <thread>
int main() {
  alignas(64) std::atomic<int> adjacent[2] {};
  alignas(64) std::atomic<int> padded_left{0};
  alignas(64) std::atomic<int> padded_right{0};
  auto elapsed = [](auto&& body) {
    const auto start = std::chrono::steady_clock::now();
    body();
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  };
  const auto adjacent_us = elapsed([&]() {
    std::thread a([&]() { for (int i = 0; i < 100000; ++i) adjacent[0].fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 100000; ++i) adjacent[1].fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
  });
  const auto padded_us = elapsed([&]() {
    std::thread a([&]() { for (int i = 0; i < 100000; ++i) padded_left.fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 100000; ++i) padded_right.fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
  });
  syslab_metric("adjacent_us", adjacent_us);
  syslab_metric("padded_us", padded_us);
  if (adjacent[0].load() + adjacent[1].load() != 200000) return syslab_fail("adjacent");
  if (padded_left.load() + padded_right.load() != 200000) return syslab_fail("padded");
  syslab_note("MICROARCH", "false sharing is a cache-line effect; the pass condition is the totals");
  return syslab_pass();
}
''',
        "检查相邻原子和分属两个对齐对象的原子都能完成递增，并记录耗时。",
        "两种布局的计数和都是 200000。",
        "相邻数组的两个元素是否落在同一缓存行取决于 ABI 布局。耗时差异不是通过条件。",
        ["false-sharing"], ["MICROARCH", "ABI"], risk="resource_intensive")


def runtime(add):
    add(
        "experiments/compiler/object-layout", "RT-001", "object-layout", "runtime.layout",
        "Print a standard-layout object and recognize a polymorphic type.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/runtime/layout.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
struct Sample { std::uint32_t first; std::uint8_t second; std::uint64_t third; };
struct Base { virtual int id() const { return 1; } virtual ~Base() = default; };
int main() {
  static_assert(std::is_standard_layout<Sample>::value, "Sample is standard layout");
  if (offsetof(Sample, first) != 0) return syslab_fail("offset");
  Base object;
  if (!std::is_polymorphic<Base>::value || object.id() != 1) return syslab_fail("polymorphic");
  syslab::write_layout<Sample>(std::cout, "Sample",
      {SYSLAB_FIELD(Sample, first), SYSLAB_FIELD(Sample, second), SYSLAB_FIELD(Sample, third)});
  syslab_note("ABI", "field offsets below are for this compiler ABI because Sample is standard layout");
  syslab_note("ABI", "the virtual table layout of Base is not a C++ standard layout");
  return syslab_pass();
}
''',
        "输出标准布局对象的大小和对齐，并确认含虚函数的类型是多态的。",
        "Sample::first 的偏移为 0。Base 是多态类型，id() 返回 1。",
        "非标准布局类型的字段偏移不是语言保证。虚表位置只出现在观察输出的标注里。",
        ["object-layout", "rtti"], ["LANG", "ABI"])

    add(
        "experiments/compiler/exception-rtti", "RT-002", "exception-and-typeid", "runtime.exception",
        "Catch a derived exception through a base handler and read its dynamic type.",
        r'''#include "experiments/common/check.hpp"
#include <typeinfo>
struct Base { virtual ~Base() = default; };
struct Derived : Base {};
int main() {
  bool caught = false;
  try {
    throw Derived();
  } catch (const Base& object) {
    caught = typeid(object) == typeid(Derived);
  }
  if (!caught) return syslab_fail("dynamic-type");
  syslab_note("LANG", "the handler matches the base type");
  syslab_note("ABI", "unwind tables and typeinfo symbols are implementation details left to the binary observation");
  return syslab_pass();
}
''',
        "通过基类处理函数捕获派生异常，并用 typeid 核对动态类型。",
        "catch (const Base&) 捕获 Derived，typeid 与 Derived 相等。",
        "异常表和 typeinfo 的二进制布局属于 ABI。本实验不比对具体符号名。",
        ["exception", "rtti"], ["LANG", "ABI"],
        ["disassembly", "stdout", "stderr", "exit_status"])

    add(
        "experiments/coroutine/fiber", "CO-001", "fiber-yield", "runtime.fiber",
        "Two fibers interleave at explicit yield points.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/concurrency/scheduler.hpp"
#include <string>
int main() {
  syslab::CooperativeScheduler scheduler;
  std::string log;
  scheduler.spawn([&]() {
    log += "A1";
    syslab::CooperativeScheduler::yield();
    log += "A2";
  });
  scheduler.spawn([&]() {
    log += "B1";
    syslab::CooperativeScheduler::yield();
    log += "B2";
  });
  scheduler.run();
  if (log != "A1B1A2B2") return syslab_fail("order");
  syslab_note("OBSERVED", "ucontext switching is cooperative and is not a kernel context switch");
  return syslab_pass();
}
''',
        "检查两个 fiber 在 yield 处交替执行。",
        "日志为 A1B1A2B2。",
        "这是 POSIX ucontext 教学切换，不能在信号处理函数里调用，也不抢占不让出的 fiber。",
        ["fiber", "ucontext"], ["ABI", "OBSERVED"])

    add(
        "experiments/coroutine/task", "CO-002", "coroutine-task", "runtime.coroutine",
        "Interleave two tasks and propagate a coroutine exception.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/runtime/coroutine.hpp"
#include <stdexcept>
#include <string>
syslab::Task sequence(syslab::CoroutineScheduler& scheduler, std::string& log, const char* first, const char* second) {
  log += first;
  co_await syslab::yield_to(scheduler);
  log += second;
}
int main() {
  std::string log;
  {
    syslab::CoroutineScheduler scheduler;
    scheduler.spawn(sequence(scheduler, log, "A", "B"));
    scheduler.spawn(sequence(scheduler, log, "C", "D"));
    scheduler.run();
  }
  if (log != "ACBD") return syslab_fail("order");
  syslab::CoroutineScheduler failing;
  failing.spawn([]() -> syslab::Task {
    throw std::runtime_error("boom");
    co_return;
  }());
  bool threw = false;
  try {
    failing.run();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!threw) return syslab_fail("exception");
  syslab_note("LANG", "a coroutine task is not a thread and does not run until the scheduler resumes it");
  return syslab_pass();
}
''',
        "检查两个协程任务按挂起点交替，并检查协程异常从 run 重新抛出。",
        "日志为 ACBD。第二个调度器的 run 抛出 runtime_error。",
        "协程不自动并行。取消只在 yield 恢复点观察，本实验不调用 cancel。",
        ["coroutine"], ["LANG"])

    add(
        "experiments/coroutine/work-stealing", "CO-003", "parallel-for", "runtime.scheduler",
        "Sum a range with the work-stealing parallel_for.",
        r'''#include "experiments/common/check.hpp"
#include "syslab/concurrency/work_stealing.hpp"
#include <atomic>
#include <cstdint>
int main() {
  std::atomic<std::uint64_t> sum{0};
  syslab::parallel_for(4, 1000, [&](std::size_t index) { sum.fetch_add(index, std::memory_order_relaxed); });
  if (sum.load() != 999ull * 1000ull / 2ull) return syslab_fail("sum");
  syslab_note("OBSERVED", "each worker has its own deque; a stolen item is still one index from the range");
  return syslab_pass();
}
''',
        "用工作窃取并行求和，检查 0 到 999 的总和。",
        "总和等于 999 * 1000 / 2。",
        "单次切片必须放进固定容量的双端队列。这个调度器是教学实现，任务体的异常会在 join 之后重新抛出。",
        ["work-stealing"], ["OBSERVED"])
