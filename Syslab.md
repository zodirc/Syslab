# SysLab 系统底层原理实验平台
## 总体架构与完整技术设计

- 文档类型：总体架构设计 / 技术设计基线
- 项目代号：SysLab
- 设计目标：以工程实践建立从高级语言、编译器、指令集、CPU、操作系统到语言运行时的系统性认知
- 主实现语言：C++20（允许针对特定实验使用 C、汇编及少量 Python）
- 主实验平台：Linux x86-64
- 对照平台：Linux AArch64
- 文档状态：完整目标架构设计

---

# 1. 项目概述

## 1.1 项目定位

SysLab 是一个面向系统编程学习与底层机制研究的实验平台。项目不以开发新的通用运行时、操作系统或编程语言为目标，也不只是收集零散的 C++ 示例，而是通过一组结构化、可重复、可观测、可验证的实验，把程序从源代码到机器执行的关键环节连接起来。

项目需要覆盖以下完整链路：

```text
源代码与语言语义
    |
    v
编译器前端、优化与代码生成
    |
    v
目标文件、链接、ABI 与机器指令
    |
    v
寄存器、执行流水线、缓存与内存子系统
    |
    v
用户态执行、系统调用与内核服务
    |
    v
进程、线程、调度、虚拟内存与同步
    |
    v
语言运行时、并发抽象与应用程序行为
```

SysLab 的核心产物不是某个单独的数据结构，而是可复现的实验、可审查的实现、可信的观测记录，以及能够解释实验现象的技术文档。

## 1.2 本质学习目标

项目完成后，学习者应能够：

1. 从 C/C++ 源码解释编译器如何生成机器代码，并能结合汇编、反汇编和 ABI 说明函数调用、对象布局及控制流。
2. 解释栈、堆、虚拟地址空间、页表、物理页、缺页异常及内存分配器之间的关系。
3. 自行实现常见分配器、对象池、线程池、锁、同步原语、协程调度器和典型无锁数据结构。
4. 解释线程与进程的创建、运行、阻塞、唤醒、调度和上下文切换过程。
5. 从 C++ 内存模型、编译器重排、CPU 内存模型和硬件指令几个层次解释原子操作、内存序和内存屏障。
6. 使用调试器、性能分析器、系统调用跟踪和内核跟踪工具验证理论，而不是仅根据一次运行结果下结论。
7. 区分语言标准保证、ABI 约定、操作系统实现细节、处理器架构保证和某一具体编译器的实现行为。
8. 形成可以迁移到其他语言、操作系统和处理器架构的底层知识框架。

## 1.3 非目标

SysLab 不承担以下目标：

- 不开发完整操作系统或通用内核。
- 不开发可替代 GCC、Clang、GDB、perf、Valgrind 的通用工具。
- 不承诺对所有 CPU、编译器和操作系统提供完全相同的实验结果。
- 不把模拟结果当作真实硬件行为。
- 不以 benchmark 数字排名作为项目主要价值。
- 不将未定义行为、数据竞争或平台偶然行为描述为语言规范保证。
- 不以封装标准库 API 代替底层机制实现。

对于需要内核源码或硬件知识的主题，优先通过现有 Linux 内核、编译器、架构手册和可观测工具研究；只有在能够明确边界且有学习价值时，才编写内核模块、QEMU 实验或裸机代码。

---

# 2. 设计原则

## 2.1 原理、实现、观测三者闭环

每个实验必须包含三个不可分割的部分：

- 原理模型：说明对象、状态、约束、执行路径和适用边界。
- 实际实现：提供可构建、可执行、可测试的代码。
- 观测验证：通过汇编、调试器、系统跟踪、硬件计数器或统计结果验证实现行为。

只写原理不构成完成；只写代码但不能解释行为也不构成完成；只有 benchmark 而没有正确性验证同样不构成完成。

## 2.2 分层而不割裂

模块按知识边界划分，但实验应允许跨层追踪。例如 mutex 实验需要关联 C++ 标准库、pthread、futex、内核等待队列、调度器和 CPU 原子指令。模块依赖应明确，实验依赖可以跨模块组合。

## 2.3 规范与实现分离

所有结论必须标注其依据类别：

- LANG：语言标准语义。
- ABI：应用二进制接口或调用约定。
- COMPILER：编译器实现行为。
- OS：操作系统接口或内核实现。
- ISA：指令集架构保证。
- MICROARCH：具体处理器微架构行为。
- OBSERVED：特定环境下实测现象。

不得将某一平台的观测结果推广为普遍规律。

## 2.4 实验可重复

实验输入、编译器版本、编译选项、CPU 型号、内核版本、运行参数、环境限制和随机种子必须记录。性能实验应支持预热、重复运行、统计分布和异常样本识别。

## 2.5 安全优先

可能触发死锁、内存破坏、无限循环、内核崩溃或资源耗尽的实验必须声明风险等级，并默认在可控环境运行。内核实验优先使用虚拟机或专用测试机。

---

# 3. 总体架构

## 3.1 架构分层

SysLab 分为七个逻辑层：

| 层级 | 名称 | 职责 |
|---|---|---|
| L1 | Machine Fundamentals | 数据表示、地址、寄存器、指令和调用栈 |
| L2 | Language & Compiler | C/C++ 语义、编译、优化、链接、ABI、对象模型 |
| L3 | CPU & Memory Hardware | ISA、缓存、原子指令、缓存一致性、内存排序 |
| L4 | Operating System | 进程、线程、虚拟内存、系统调用、调度与内核同步 |
| L5 | Runtime & Concurrency | 分配器、线程池、锁、协程、无锁结构和对象生命周期 |
| L6 | Experiment & Observation | 实验编排、运行控制、跟踪、数据采集、分析和报告 |
| L7 | Knowledge & Verification | 原理文档、实验契约、结论、引用依据和验证记录 |

逻辑层不代表运行时必须存在同样的七层软件调用。它用于组织知识、设计实验和表达依赖关系。

## 3.2 系统组件

```text
                         SysLab
                           |
          +----------------+----------------+
          |                |                |
      Experiment       Observation       Knowledge
       Manager           Engine           Registry
          |                |                |
   +------+------+   +-----+------+   +-----+------+
   |      |      |   |     |      |   |     |      |
  Build  Runner  Env Trace Metrics Report Theory  Spec
   |      |      |   |     |      |   |     |      |
   +------+------+   +-----+------+   +-----+------+
          |                |                |
          +----------------+----------------+
                           |
                     Experiment API
                           |
      +----------+---------+---------+-----------+
      |          |         |         |           |
    CPU/ABI    Memory   OS/Process  Concurrency  Runtime
      |          |         |         |           |
      +----------+---------+---------+-----------+
                           |
                  Platform Adapters
                           |
          Linux x86-64 / Linux AArch64 / QEMU
```

## 3.3 组件职责

### Experiment Manager

负责实验注册、参数校验、依赖检查、构建配置选择、运行计划、超时控制、结果归档和实验状态管理。它不负责解释实验原理，也不直接实现底层机制。

### Build Manager

负责调用 CMake、编译器和链接器，生成可追溯构建记录。每次构建记录编译器路径、版本、目标架构、标准版本、编译选项、链接选项、依赖版本和构建产物摘要。

### Runner

在受控环境中启动实验进程，设置环境变量、CPU 亲和性、资源限制、超时和输出捕获。对有风险的实验可使用容器或虚拟机隔离。Runner 不应通过 shell 拼接用户输入执行命令；参数必须使用结构化 argv 传递。

### Observation Engine

统一管理汇编、反汇编、系统调用、线程状态、性能计数器、内存映射和日志等观测数据。观测工具不可用时应明确标记为 unsupported 或 unavailable，不得伪造数据。

### Knowledge Registry

保存模块、概念、实验、术语、依赖和规范依据之间的关联。它是知识导航与实验追溯的索引，不替代 Markdown 技术文档。

### Platform Adapters

封装 Linux、x86-64、AArch64、编译器及性能工具的差异。平台适配层只提供能力检测和标准化接口，不抹平必须被学习者观察的架构差异。

---

# 4. 功能模块设计

# 4.1 CPU、寄存器与汇编模块

## 4.1.1 目标

建立源代码、ABI、汇编指令和处理器状态之间的映射关系。

## 4.1.2 知识范围

- 二进制与补码、整数溢出和浮点表示。
- 通用寄存器、指令指针、栈指针、标志寄存器。
- x86-64 指令编码、寻址方式和指令类别。
- ARM64 通用寄存器、PC、SP、NZCV 和寻址模式。
- CALL、RET、跳转、条件分支和函数序言/尾声。
- System V AMD64 ABI、AArch64 Procedure Call Standard。
- 栈帧、栈对齐、参数传递、返回值传递。
- 内联汇编与独立汇编文件。
- ELF、符号表、重定位、动态链接和 PLT/GOT。
- 编译器优化、内联、尾调用、寄存器分配和指令选择。

## 4.1.3 实验集合

| 实验 ID | 实验名称 | 关键验证 |
|---|---|---|
| CPU-001 | 整数与浮点数据表示 | 内存字节、补码、IEEE 754 |
| CPU-002 | 函数调用与返回 | 参数寄存器、返回寄存器、栈变化 |
| CPU-003 | 栈帧与局部变量 | O0/O2 下栈帧差异 |
| CPU-004 | 调用约定对比 | x86-64 ABI 与 AArch64 ABI |
| CPU-005 | 虚函数调用指令路径 | 间接寻址与间接调用 |
| CPU-006 | ELF 文件结构 | section、segment、symbol、relocation |
| CPU-007 | 动态链接路径 | PLT、GOT、动态加载 |
| CPU-008 | 编译器优化观察 | 常量传播、内联、死代码消除 |
| CPU-009 | 原子指令反汇编 | 不同 memory_order 的指令差异 |
| CPU-010 | 条件分支与分支预测 | 分支模式与性能计数器 |

## 4.1.4 实现要求

提供 `syslab inspect asm`、`syslab inspect elf`、`syslab inspect layout` 等命令。工具负责组织已有工具的输出、记录环境和生成实验材料，不重新实现完整反汇编器或 ELF 解析器。必要时可实现有限的 ELF 元数据读取器，用于教学展示。

---

# 4.2 内存管理模块

## 4.2.1 目标

理解语言层对象生命周期、用户态分配器、虚拟内存和物理页管理之间的边界。

## 4.2.2 知识范围

- 自动存储期、静态存储期、动态存储期和线程存储期。
- 栈帧、堆分配、对象生命周期和对齐。
- malloc/free、new/delete、placement new。
- 分配器元数据、空闲链表、分块、合并和碎片。
- Arena、Pool、Slab、Buddy 等分配策略。
- mmap、munmap、brk、mprotect。
- 虚拟地址空间、页表、多级页表、TLB。
- Page Fault、Demand Paging、Copy-on-Write。
- RSS、VSS、PSS、映射区域和驻留页。
- RAII、unique_ptr、shared_ptr、weak_ptr。
- 引用计数、控制块和线程安全析构。
- 内存泄漏、越界、重复释放和 Use-After-Free。

## 4.2.3 子模块

### Allocator Core

定义统一分配器接口，并实现多种策略。底层实验分配器不得默认替换全局 new/delete，以免破坏测试框架和标准库运行。全局替换仅作为隔离实验。

### Virtual Memory Lab

通过 mmap、munmap、mprotect 等系统接口研究虚拟内存。提供页粒度映射实验、访问触发缺页实验、保护权限实验和映射释放实验。

### Object Lifetime Lab

研究对象构造、析构、移动、异常安全、引用计数和并发析构。需要区分对象生命周期与其承载内存的生命周期。

### Memory Diagnostics

集成 ASan、LSan、UBSan、Valgrind 和自定义分配器校验器。诊断工具结果必须保留原始输出和工具版本。

## 4.2.4 分配器接口

```cpp
struct AllocationRequest {
    std::size_t size;
    std::size_t alignment;
    std::uint32_t tag;
};

struct AllocationStats {
    std::size_t reserved_bytes;
    std::size_t committed_bytes;
    std::size_t allocated_bytes;
    std::size_t free_bytes;
    std::size_t allocation_count;
    std::size_t deallocation_count;
    std::size_t peak_allocated_bytes;
};

class IAllocator {
public:
    virtual ~IAllocator() = default;

    virtual void* allocate(const AllocationRequest& request) = 0;
    virtual void deallocate(void* ptr,
                            std::size_t size,
                            std::size_t alignment) = 0;
    virtual AllocationStats statistics() const = 0;
};
```

接口是教学实验抽象，不意味着所有分配器都能以相同方式报告 reserved、committed 等指标。不可提供的字段应明确标记，而不是填入推测值。

## 4.2.5 必须实现的分配器

1. Linear/Arena Allocator。
2. Fixed-size Pool Allocator。
3. Free-list Allocator。
4. Buddy Allocator。
5. Size-class Allocator。
6. Thread-local Cache Allocator 实验版本。

每个实现需要处理溢出检查、对齐、元数据损坏检测、重复释放检测和统计一致性。教学实现应明确不适用于生产环境的限制。

---

# 4.3 进程、线程与操作系统模块

## 4.3.1 目标

从用户态程序的可观察行为理解 Linux 进程、线程、系统调用、调度和上下文切换。

## 4.3.2 知识范围

- 用户态和内核态。
- 系统调用入口与返回。
- 进程地址空间、线程组和 task_struct。
- fork、exec、wait、clone/clone3。
- pthread 创建与销毁。
- 进程间通信：pipe、socket、共享内存、eventfd。
- 线程调度、抢占、时间片、优先级和 CPU affinity。
- 阻塞、唤醒、等待队列。
- 用户态上下文切换与内核线程切换。
- 上下文保存、恢复和调度路径。
- 信号、信号屏蔽和异步处理限制。
- NUMA 与 CPU 拓扑。

## 4.3.3 重要概念边界

- 进入内核态不等于发生上下文切换。
- 系统调用可能在当前线程上下文中完成并返回。
- 线程阻塞后通常需要调度其他可运行任务，但具体调度结果由内核决定。
- Linux 中进程与线程都以 task 为核心调度实体，用户态 API 的抽象不应与内核数据结构简单等同。
- `fork()` 的地址空间复制通常利用 Copy-on-Write，不等于立即复制所有物理页。
- 用户态协程切换不等于内核线程上下文切换。

## 4.3.4 实验集合

| 实验 ID | 实验名称 | 主要工具 |
|---|---|---|
| OS-001 | 系统调用观察 | strace |
| OS-002 | fork 与 Copy-on-Write | /proc、perf、缺页统计 |
| OS-003 | 线程创建与销毁 | pthread、strace |
| OS-004 | 进程与线程地址空间对比 | /proc/PID/maps |
| OS-005 | 阻塞与唤醒 | perf sched、ftrace |
| OS-006 | CPU 亲和性 | sched_setaffinity、taskset |
| OS-007 | 上下文切换统计 | getrusage、perf |
| OS-008 | IPC 性能与语义 | pipe、socket、共享内存 |
| OS-009 | 信号交付与处理 | sigaction、strace |
| OS-010 | NUMA 访问差异 | numactl、perf |

## 4.3.5 内核观测策略

优先使用 perf、ftrace、tracefs、/proc、/sys、strace 和内核文档。涉及内核函数名、调度器实现和结构体布局时，必须记录内核版本。不得把某一版本的内部函数调用路径写成稳定 ABI。

---

# 4.4 同步原语与锁模块

## 4.4.1 目标

从互斥、等待、唤醒和调度角度理解同步原语的语义与实现。

## 4.4.2 实现清单

- Test-and-Set Spinlock。
- Ticket Lock。
- MCS Lock。
- Mutex。
- Read-Write Lock。
- Recursive Mutex。
- Condition Variable。
- Semaphore。
- Barrier。
- Futex-based Mutex。
- Once Initialization。

## 4.4.3 设计要求

每种同步原语必须定义：

- 状态表示。
- 允许的状态转换。
- 获取与释放语义。
- 等待策略：自旋、阻塞或混合。
- 公平性保证。
- 可重入性。
- 错误处理方式。
- 销毁前置条件。
- 线程取消和异常退出时的行为边界。
- 内存序要求。
- 可验证的不变量。

## 4.4.4 Mutex 实验路径

```text
C++ lock_guard / unique_lock
            |
            v
      std::mutex API
            |
            v
   标准库平台实现（pthread）
            |
            v
     用户态原子快路径
            |
      +-----+------+
      |            |
   获取成功      竞争失败
      |            |
      v            v
  临界区执行    futex 等待路径
                   |
                   v
             内核等待与唤醒
                   |
                   v
               重新竞争
```

实际路径取决于标准库、libc、内核版本和锁状态。实验报告必须通过反汇编、系统调用跟踪和竞争条件对比确认具体路径。

---

# 4.5 原子操作、内存模型与无锁编程模块

## 4.5.1 目标

建立语言内存模型、编译器重排、处理器内存模型和硬件同步指令之间的准确联系。

## 4.5.2 知识范围

- 原子对象与原子操作。
- Read-Modify-Write。
- Compare-and-Swap。
- Exchange、Fetch Add、Test-and-Set。
- C++ happens-before、synchronizes-with、sequenced-before。
- Relaxed、Acquire、Release、Acq_rel、Seq_cst。
- Fence 与 atomic fence。
- 编译器屏障与硬件屏障。
- x86 TSO 与 ARM 弱内存模型。
- Store Buffer、Load Buffer 和缓存一致性。
- MESI 类协议的概念模型。
- False Sharing。
- ABA。
- Lock-free、Wait-free、Obstruction-free。
- Hazard Pointer、Epoch-based Reclamation、RCU 概念。

## 4.5.3 必须严格区分的屏障

文档和代码中不得笼统使用“内存屏障”而不说明层次：

1. 编译器屏障：约束编译器优化和重排。
2. C++ 原子 fence：参与语言内存模型的同步关系。
3. ISA 屏障指令：处理器架构定义的内存访问排序约束。
4. 缓存一致性机制：协调多个核心对缓存行的观察与所有权。
5. 设备内存与 DMA 屏障：涉及设备可见性和平台特定规则。

这些机制有联系，但不是同义词。volatile 不能代替原子操作或线程同步。

## 4.5.4 实验集合

| 实验 ID | 实验名称 | 验证重点 |
|---|---|---|
| AT-001 | 非原子数据竞争 | TSan、未定义行为边界 |
| AT-002 | Relaxed 原子计数 | 原子性与排序的区别 |
| AT-003 | Message Passing | Release/Acquire |
| AT-004 | Store Buffering | 内存模型与架构差异 |
| AT-005 | Seq_cst 全序 | 顺序一致性约束 |
| AT-006 | Fence 实验 | fence 与原子操作关系 |
| AT-007 | CAS 与 ABA | 指针复用问题 |
| AT-008 | False Sharing | 缓存行竞争 |
| AT-009 | Lock-free Stack | CAS 循环与回收安全 |
| AT-010 | SPSC Ring Buffer | 单生产者单消费者协议 |
| AT-011 | MPMC Queue | 多生产者多消费者正确性 |
| AT-012 | Hazard Pointer | 延迟回收与安全扫描 |
| AT-013 | Epoch Reclamation | 宽限期与回收条件 |
| AT-014 | x86/ARM 对照 | 指令和允许行为差异 |

## 4.5.5 无锁结构要求

所有无锁结构必须提供：

- 算法状态图。
- 线性化点说明。
- 关键不变量。
- 内存回收策略。
- ABA 风险分析。
- 进度保证分类。
- 压力测试。
- 线性化或历史记录验证。
- Sanitizer 测试。
- 多架构验证记录。

不能因为代码使用 `std::atomic` 就称其为 lock-free。需要检查目标类型的 `is_lock_free()`，并区分算法层面的 lock-free 进度保证与底层原子类型实现能力。

---

# 4.6 C++ 语言对象模型与运行时模块

## 4.6.1 知识范围

- 对象表示、对象布局和对齐。
- 生命周期与存储期。
- 构造、析构、拷贝和移动。
- 虚函数表、虚基类和多重继承。
- RTTI 与异常处理实现。
- 模板实例化和链接。
- Lambda 闭包对象。
- thread_local。
- 智能指针与控制块。
- 协程语法、协程帧和 promise_type。
- 标准库线程、锁、原子和分配器抽象。

## 4.6.2 实现内容

### Object Layout Inspector

提供用于观察对象大小、对齐、字段偏移和虚函数相关布局的实验工具。对于标准未规定的布局，输出必须标注 ABI 和编译器条件。

### Smart Pointer Lab

实现教学版本的 unique ownership、shared ownership 和 weak ownership。重点验证异常安全、控制块生命周期、原子引用计数和循环引用。

### Coroutine Lab

实现一个最小协程调度器，展示协程帧、挂起点、恢复点和 promise 生命周期。明确协程本身不等同于线程，也不自动提供并行执行能力。

### Exception & RTTI Lab

通过编译产物、符号、反汇编和调试器观察异常展开及运行时类型信息。不得将具体 ABI 实现描述为 C++ 标准固定布局。

---

# 4.7 性能、缓存与硬件行为模块

## 4.7.1 知识范围

- Cache hierarchy、Cache Line。
- Cache hit/miss。
- TLB 与页表遍历。
- Branch prediction。
- Out-of-order execution 概念。
- Store buffer。
- Memory bandwidth 与 latency。
- NUMA。
- False Sharing。
- 原子竞争。
- SIMD 与向量化。
- CPU performance counters。

## 4.7.2 实验要求

每个性能实验必须包含正确性测试和性能测试两部分。性能数据必须记录 CPU 型号、微码（可获得时）、内核版本、编译器版本、频率策略、NUMA 位置、线程亲和性和测试时长。

实验应报告中位数、分位数或置信区间等适当统计量，不得只报告单次最优值。对硬件计数器事件应记录事件名称和可用性，避免不同微架构上同名事件语义不一致。

---

# 4.8 用户态调度与协程模块

## 4.8.1 目标

通过实现用户态执行上下文和调度器，区分内核线程调度与语言运行时调度。

## 4.8.2 实现内容

- 最小 Fiber。
- 独立栈与栈生命周期。
- 上下文保存和恢复。
- Cooperative Scheduler。
- Timer/IO 等待抽象。
- Coroutine Task。
- Work Queue。
- Work Stealing。
- 多线程调度器。
- 协程取消与异常传播。

## 4.8.3 安全边界

上下文切换涉及 ABI、寄存器保存、栈对齐、信号和异常处理等复杂约束。优先使用经过验证的上下文切换实现或编译器协程机制，再实现教学版本。若编写汇编切换代码，必须按目标 ABI 保存所有要求保留的寄存器并验证栈对齐。不得把未经验证的手写上下文切换代码用于生产环境。

---

# 5. 实验平台与统一执行框架

## 5.1 实验模型

每个实验以独立 manifest 注册。manifest 描述实验身份、构建目标、运行参数、依赖能力、风险等级、观测项、验证规则和产物。

示例：

```yaml
schema_version: 1

experiment:
  id: AT-003
  name: release-acquire-message-passing
  module: atomic.memory-model
  description: Demonstrate message passing synchronization.
  risk: normal

platform:
  os: linux
  architectures:
    - x86_64
    - aarch64

build:
  target: exp_at_003
  standard: c++20
  variants:
    - name: optimized
      build_type: Release
    - name: debug
      build_type: Debug

run:
  executable: exp_at_003
  timeout_ms: 10000
  repetitions: 100
  arguments: []

observe:
  - disassembly
  - compiler_metadata
  - stdout
  - stderr
  - exit_status

verify:
  checks:
    - name: completed
      type: exit_code
      expected: 0

artifacts:
  preserve:
    - executable
    - build_log
    - run_log
    - disassembly
    - environment
```

Manifest 仅描述执行配置，不允许任意 shell 脚本在高权限环境中运行。复杂实验可以使用专门的 C++ 驱动程序，并由 manifest 引用已注册的 runner 类型。

## 5.2 实验生命周期

```text
REGISTERED
    |
    v
VALIDATING
    |
    v
ENVIRONMENT_CHECK
    |
    v
BUILDING
    |
    v
BUILT
    |
    v
RUNNING
    |
    v
OBSERVING
    |
    v
VERIFYING
    |
    +------> PASSED
    |
    +------> FAILED
    |
    +------> UNSUPPORTED
    |
    +------> INTERRUPTED
```

状态含义：

- REGISTERED：实验元数据已加载。
- VALIDATING：检查 manifest、参数和依赖。
- ENVIRONMENT_CHECK：检查系统能力和工具。
- BUILDING：编译目标。
- BUILT：构建成功。
- RUNNING：实验进程执行中。
- OBSERVING：采集执行证据。
- VERIFYING：执行实验断言。
- PASSED：所有必要断言通过。
- FAILED：实验或验证失败。
- UNSUPPORTED：平台不支持所需能力。
- INTERRUPTED：被用户或系统中断。

任何状态转换都应记录时间戳和原因。失败不能被自动转换为通过。

## 5.3 统一命令行

```bash
syslab list
syslab list --module memory

syslab show AT-003
syslab validate AT-003
syslab build AT-003 --variant optimized
syslab run AT-003 --repeat 100
syslab inspect AT-003 --artifact disassembly
syslab report AT-003 --latest

syslab env
syslab env --capabilities
syslab toolchain list
syslab toolchain inspect gcc
```

命令行参数必须具备稳定语义。输出支持人类可读格式和 JSON 格式，方便后续自动分析。

## 5.4 运行隔离

实验风险分级：

| 等级 | 类型 | 默认执行环境 |
|---|---|---|
| R0 | 只读检查、静态分析 | 当前用户 |
| R1 | 普通用户态程序 | 当前用户，带超时 |
| R2 | 资源密集、死锁、内存破坏实验 | 受限子进程或容器 |
| R3 | 内核跟踪、内核模块、特权接口 | 专用虚拟机或测试机 |
| R4 | 裸机、设备和可能导致系统失效的实验 | 独立硬件或可恢复虚拟环境 |

Runner 必须支持超时、输出大小限制、进程组清理、CPU 时间限制和内存限制。高风险实验不得通过简单的命令行标志绕过隔离策略。

---

# 6. 核心数据结构与接口

## 6.1 ExperimentDescriptor

```cpp
enum class RiskLevel {
    ReadOnly,
    Normal,
    ResourceIntensive,
    KernelPrivileged,
    BareMetal
};

struct ExperimentDescriptor {
    std::string id;
    std::string name;
    std::string module;
    std::string description;
    std::uint32_t schema_version;
    RiskLevel risk;
    std::vector<std::string> tags;
    std::vector<std::string> supported_platforms;
    std::vector<std::string> required_capabilities;
};
```

## 6.2 EnvironmentSnapshot

```cpp
struct EnvironmentSnapshot {
    std::string os_name;
    std::string kernel_release;
    std::string architecture;
    std::string cpu_model;
    std::string compiler_id;
    std::string compiler_version;
    std::string standard_library;
    std::string libc_version;
    std::string page_size;
    std::string cpu_topology;
    std::string environment_id;
};
```

字段无法采集时使用显式的 unavailable 状态或可空字段，并附带采集原因。不得以空字符串混淆“未知”“不支持”和“采集失败”。

## 6.3 BuildRecord

```cpp
struct BuildRecord {
    std::string build_id;
    std::string experiment_id;
    std::string compiler_path;
    std::string compiler_version;
    std::string target_triple;
    std::string language_standard;
    std::vector<std::string> compile_arguments;
    std::vector<std::string> link_arguments;
    std::string source_revision;
    std::string artifact_digest;
    std::string started_at;
    std::string completed_at;
    int exit_code;
};
```

## 6.4 RunRecord

```cpp
struct RunRecord {
    std::string run_id;
    std::string experiment_id;
    std::string build_id;
    std::string environment_id;
    std::string started_at;
    std::string completed_at;
    std::uint64_t duration_ns;
    int exit_code;
    bool timed_out;
    bool interrupted;
    std::string stdout_artifact;
    std::string stderr_artifact;
};
```

## 6.5 ObservationRecord

```cpp
enum class ObservationKind {
    Disassembly,
    SyscallTrace,
    SchedulerTrace,
    PerformanceCounter,
    MemoryMap,
    ThreadState,
    AllocatorStatistics,
    SanitizerReport,
    DebuggerSnapshot,
    StandardOutput,
    StandardError
};

struct ObservationRecord {
    std::string observation_id;
    std::string run_id;
    ObservationKind kind;
    std::string tool_name;
    std::string tool_version;
    std::string artifact_path;
    std::string content_digest;
    std::string collected_at;
    bool complete;
    std::string limitation;
};
```

## 6.6 VerificationResult

```cpp
enum class VerificationStatus {
    Passed,
    Failed,
    Unsupported,
    Inconclusive
};

struct VerificationResult {
    std::string check_id;
    std::string run_id;
    VerificationStatus status;
    std::string expected;
    std::string actual;
    std::string explanation;
    std::vector<std::string> evidence_ids;
};
```

## 6.7 数据存储原则

- 实验定义采用 YAML 或 JSON，必须进行 schema 校验。
- 实验源码、说明和理论文档采用 Markdown、C/C++ 和汇编源文件管理。
- 运行元数据采用 JSON，便于版本控制和脚本处理。
- 大型跟踪文件、二进制文件和性能原始数据存入 artifacts 目录，不提交所有运行产物到 Git。
- 每个产物记录 SHA-256 摘要、生成工具、版本和关联 run_id。
- 结果不可覆盖；重新运行生成新的 run_id。
- 对同一实验的多次执行通过 experiment_id、build_id、environment_id 和 run_id 关联。

---

# 7. 目录结构

```text
syslab/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── .clang-format
├── .clang-tidy
├── .gitignore
│
├── docs/
│   ├── architecture/
│   │   ├── overview.md
│   │   ├── module-boundaries.md
│   │   ├── experiment-lifecycle.md
│   │   ├── data-model.md
│   │   ├── platform-support.md
│   │   └── security-model.md
│   ├── concepts/
│   │   ├── cpu/
│   │   ├── compiler/
│   │   ├── memory/
│   │   ├── operating-system/
│   │   ├── concurrency/
│   │   ├── atomic/
│   │   └── cpp-runtime/
│   ├── experiments/
│   ├── references/
│   └── decisions/
│
├── schemas/
│   ├── experiment.schema.json
│   └── report.schema.json
│
├── include/syslab/
│   ├── core/
│   ├── experiment/
│   ├── build/
│   ├── runner/
│   ├── observation/
│   ├── platform/
│   ├── memory/
│   ├── synchronization/
│   ├── atomic/
│   ├── concurrency/
│   └── runtime/
│
├── src/
│   ├── cli/
│   ├── core/
│   ├── experiment/
│   ├── build/
│   ├── runner/
│   ├── observation/
│   ├── platform/linux/
│   ├── platform/x86_64/
│   ├── platform/aarch64/
│   ├── memory/
│   ├── synchronization/
│   ├── atomic/
│   ├── concurrency/
│   └── runtime/
│
├── experiments/
│   ├── cpu/
│   ├── assembly/
│   ├── abi/
│   ├── compiler/
│   ├── memory/
│   ├── process/
│   ├── thread/
│   ├── synchronization/
│   ├── atomic/
│   ├── lockfree/
│   ├── cache/
│   ├── coroutine/
│   └── kernel/
│
├── tools/
│   ├── report/
│   ├── disassembly/
│   ├── elf-inspector/
│   └── data-analysis/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── stress/
│   ├── sanitizer/
│   └── platform/
│
├── benchmarks/
├── scripts/
├── third_party/
└── artifacts/                 # 默认忽略，不提交运行数据
```

实验目录中的每个实验应有独立子目录：

```text
experiments/atomic/message-passing/
├── experiment.yaml
├── README.md
├── main.cpp
├── variants/
│   ├── relaxed.cpp
│   ├── release_acquire.cpp
│   └── seq_cst.cpp
├── expected/
│   └── behavior.md
└── tests/
    └── validation.cpp
```

---

# 8. 构建、依赖与技术选型

## 8.1 语言与标准

- 主语言：C++20。
- 特定底层接口：C17。
- 汇编：GNU assembler 语法；必要时支持 Intel 与 AT&T 语法展示。
- 脚本：Python 3，用于数据处理、报告和辅助自动化。
- 文档：Markdown。
- 实验描述：YAML。

C++23 可作为可选实验标准，但不能让核心框架依赖仅在部分编译器上支持的特性。涉及标准库实现差异的实验，应记录 libstdc++、libc++ 等具体实现。

## 8.2 工具链

- CMake：构建系统。
- Ninja：高效构建后端。
- GCC、Clang：至少两套编译器验证。
- GDB、LLDB：调试。
- objdump、readelf、nm：二进制分析。
- strace：系统调用跟踪。
- perf：性能和调度分析。
- ftrace/tracefs：内核跟踪。
- ASan、UBSan、TSan、LSan：动态检测。
- Valgrind：内存与线程分析。
- QEMU：架构模拟及隔离实验。

工具能力需要在启动时探测。不能假定所有 Linux 发行版都安装了全部工具，也不能假定 perf 事件对当前 CPU 可用。

## 8.3 构建配置

至少提供：

- Debug：调试符号、低优化。
- Release：常规优化。
- RelWithDebInfo：优化与调试符号并存。
- ASan/UBSan。
- TSan。
- Coverage。
- Architecture-specific：x86-64、AArch64。

不允许将 TSan 与 ASan 等不兼容组合强行合并。不同 sanitizer 使用独立构建目录。

## 8.4 第三方依赖原则

核心实验尽量依赖标准库和系统接口。框架可使用成熟的 YAML/JSON 解析库和测试框架，但依赖必须固定版本并记录许可证。不能为了实现一个简单机制引入大型框架，避免依赖本身遮蔽学习目标。

---

# 9. 测试与质量保证

## 9.1 测试层次

### 单元测试

验证分配器元数据、空闲块合并、队列状态机、计数器、参数解析和数据模型。

### 集成测试

验证实验注册、构建、运行、观测、验证和报告的完整链路。

### 压力测试

验证高并发、长时间运行、资源竞争、异常退出和反复创建销毁。

### Sanitizer 测试

使用 ASan、UBSan、TSan、LSan 检测内存错误、未定义行为和数据竞争。Sanitizer 不能证明程序正确，也不能替代算法证明。

### 跨平台测试

至少在 x86-64 Linux 上完整执行；AArch64 对支持的实验执行。实验不支持某平台时必须给出明确原因。

## 9.2 并发正确性测试

无锁和同步原语必须使用多种方式验证：

- 状态不变量检查。
- 高并发压力测试。
- 随机调度扰动。
- 线性化历史检查。
- 超时和死锁检测。
- ThreadSanitizer（适用时）。
- 不同编译器和优化等级测试。
- x86-64 与 AArch64 对照。

压力测试未复现错误不等于证明不存在错误。

## 9.3 性能测试规范

- 正确性测试与性能测试分离。
- 固定输入规模和参数。
- 记录预热策略。
- 多轮执行。
- 记录中位数和离散程度。
- 控制 CPU 亲和性和 NUMA 位置。
- 记录频率调节和系统负载。
- 避免把调试构建结果用于性能结论。
- 不以不同机器上的绝对数字直接比较实现优劣。

---

# 10. 实验报告规范

每次实验生成一份报告，结构固定：

1. 实验目的。
2. 对应知识点。
3. 理论模型。
4. 关键术语与规范依据。
5. 实验环境。
6. 实现说明。
7. 构建参数。
8. 执行步骤。
9. 原始观测数据。
10. 汇编或系统调用分析。
11. 正确性验证。
12. 性能结果（如适用）。
13. 现象解释。
14. 与理论不一致的地方。
15. 平台和实现限制。
16. 结论。
17. 参考资料。

结论应区分：

- 已由语言规范保证。
- 由 ABI 或 ISA 保证。
- 由当前操作系统或库实现决定。
- 仅在当前实验环境中观察到。
- 尚未验证或存在争议。

报告生成器不得自行推断因果关系。自动分析结果应标注为工具输出，最终解释由实验文档给出。

---

# 11. 知识体系与依赖关系

模块依赖不是简单的线性课程顺序。以下依赖图用于保证知识前置关系：

```text
数据表示、指针、地址
        |
        +------> C/C++ 对象模型
        |              |
        |              +----> 生命周期与分配器
        |              |
        |              +----> 虚函数、RTTI、异常
        |
        +------> 汇编与寄存器
                       |
                       +----> ABI 与函数调用
                       |
                       +----> 原子指令与上下文切换
                                      |
编译器优化与语言内存模型 ---------------+
        |                             |
        v                             v
C++ 原子与 happens-before          CPU 内存模型
        |                             |
        +-------------+---------------+
                      |
                      v
             锁与无锁算法
                      |
                      v
             并发运行时与调度

虚拟内存与系统调用
        |
        +----> 进程与线程
        |
        +----> 分配器底层实现
        |
        +----> Futex 与内核同步
                      |
                      v
                调度与上下文切换

CPU Cache / TLB / NUMA
        |
        +----> 内存性能
        +----> False Sharing
        +----> 并发性能
```

每个实验应声明前置实验和关联概念。知识图谱只表达依赖，不限制学习者按不同路径探索。

---

# 12. 平台适配与架构支持

## 12.1 第一类：语言与编译器实验

尽量跨平台。通过不同编译器、标准库和目标架构的构建产物进行对比。

## 12.2 第二类：操作系统实验

以 Linux 为主。系统调用、调度、futex、/proc 和内核跟踪属于 Linux 平台能力，不应伪装成跨操作系统通用接口。

## 12.3 第三类：ISA 实验

按架构分别组织代码：

```text
src/platform/
├── x86_64/
│   ├── atomic/
│   ├── context/
│   └── barriers/
└── aarch64/
    ├── atomic/
    ├── context/
    └── barriers/
```

公共接口只封装实验需要的能力。架构差异必须能够在实验输出中直接观察。

## 12.4 QEMU 的定位

QEMU 用于架构模拟、隔离和部分低层实验。模拟器结果不能直接替代真实 CPU 的性能结果，也不能证明微架构行为与真实处理器一致。涉及性能、缓存和分支预测的结论必须在真实硬件上验证。

---

# 13. 安全、资源与异常处理

## 13.1 实验进程管理

Runner 应具备：

- 独立进程组管理。
- 超时终止。
- SIGTERM 后 SIGKILL 兜底。
- stdout/stderr 文件大小限制。
- CPU 时间与内存限制。
- 子进程清理。
- 运行目录隔离。
- 环境变量白名单或显式配置。
- 退出码、信号和超时原因记录。

## 13.2 资源限制

实验可以声明最大内存、最大运行时间、最大线程数和最大输出大小。超过限制时应标记为资源限制终止，而不是普通实验失败。

## 13.3 内核实验

内核模块、特权跟踪和可能改变系统状态的实验默认不在日常主机上运行。必须提供虚拟机快照、恢复步骤和风险说明。任何实验都不得要求关闭系统安全机制作为默认前提。

---

# 14. 文档与代码工程规范

## 14.1 文档层级

- README：项目定位、构建方式、快速入口。
- 架构文档：组件、边界、依赖和数据流。
- 模块设计：模块职责、接口和关键数据结构。
- 概念文档：解释原理、规范和实现差异。
- 实验文档：操作步骤、观察方法、结果解释。
- ADR：记录重要技术决策及替代方案。
- 参考资料：标准、ABI、ISA 手册、内核文档和工具手册。

## 14.2 代码规范

- 使用明确的命名空间和模块边界。
- 公共接口提供 Doxygen 风格注释，采用 `@brief`、`@param`、`@return`、`@throws` 等标签。
- 注释解释约束、原因和不变量，不重复代码表面含义。
- 资源管理优先使用 RAII。
- 不使用裸 `new/delete` 作为常规业务式资源管理方式；底层分配器实验除外。
- 错误处理必须明确，不能吞掉异常或忽略系统调用返回值。
- 对系统调用检查 errno。
- 对尺寸计算和地址计算执行溢出检查。
- 并发代码必须说明共享状态、同步关系和内存序理由。
- 汇编代码必须说明 ABI 假设、寄存器约束和平台范围。
- 代码修改必须同步更新接口文档、实验说明和相关图示。

## 14.3 变更闭合

每项功能变更必须完成：

1. 需求与实验目标更新。
2. 接口和数据结构更新。
3. 实现。
4. 单元测试和集成测试。
5. 必要的压力测试、Sanitizer 和平台测试。
6. 实验文档与架构文档更新。
7. 构建和运行验证。
8. 变更记录。

不得以“核心逻辑已完成”代替完整交付。

---

# 15. 完整能力验收标准

项目达到完整目标时，应满足以下验收条件。

## 15.1 知识覆盖

- [ ] 能从源码追踪到汇编和 ABI。
- [ ] 能解释寄存器、栈帧和函数调用。
- [ ] 能观察 C++ 对象布局与虚函数派发。
- [ ] 能实现并比较多种分配器。
- [ ] 能解释虚拟内存、页表和缺页。
- [ ] 能解释进程、线程、系统调用和调度。
- [ ] 能实现多种锁和同步原语。
- [ ] 能解释 C++ 内存模型和 happens-before。
- [ ] 能实现并验证无锁数据结构。
- [ ] 能解释 CPU 缓存、缓存一致性和伪共享。
- [ ] 能实现用户态协程或 Fiber 调度器。
- [ ] 能对比 x86-64 与 AArch64 的关键差异。

## 15.2 工程能力

- [ ] 所有实验具有统一 manifest。
- [ ] 实验可通过 CLI 注册、构建、运行和报告。
- [ ] 环境、编译参数和产物可追溯。
- [ ] 实验支持超时和资源限制。
- [ ] 观测数据与实验运行记录关联。
- [ ] 失败、未支持和结论不确定有明确状态。
- [ ] 具有单元、集成、压力和跨平台测试。
- [ ] 文档、代码和实验结果保持同步。

## 15.3 学习闭环

- [ ] 每个核心机制至少有一个最小实现。
- [ ] 每个核心机制至少有一个对照实验。
- [ ] 关键实验能提供汇编或系统调用证据。
- [ ] 并发实验有正确性验证，不只看输出。
- [ ] 性能实验有环境记录和统计方法。
- [ ] 结论区分规范保证与实现观察。
- [ ] 能从实验现象反向解释底层机制。

---

# 16. 参考资料体系

参考资料应优先使用规范、官方手册和一手实现资料。

## 16.1 语言与 ABI

- ISO C++ 标准及公开草案。
- ISO C 标准及公开草案。
- Itanium C++ ABI。
- System V AMD64 ABI。
- AArch64 Procedure Call Standard。
- GCC 与 Clang 官方文档。

## 16.2 处理器与汇编

- Intel 64 and IA-32 Architectures Software Developer's Manual。
- AMD64 Architecture Programmer's Manual。
- Arm Architecture Reference Manual。
- GNU Binutils 文档。
- ELF ABI 文档。

## 16.3 操作系统

- Linux man-pages。
- Linux Kernel Documentation。
- Linux 内核源码。
- perf、ftrace、tracefs 官方文档。
- POSIX 标准相关文档。

## 16.4 并发与内存模型

- C++ concurrency memory model 相关标准章节。
- Linux futex man page 与内核实现。
- Linux 内核 memory barriers 文档。
- 处理器厂商关于原子指令与内存排序的架构手册。
- 无锁算法、Hazard Pointer、Epoch Reclamation 的原始论文及可靠技术资料。

引用资料应记录版本、章节和访问日期。涉及内核内部实现时，应固定到具体内核版本或 commit。

---

# 17. 最终交付定义

SysLab 的最终交付由以下部分构成：

1. **底层机制实验库**：CPU、汇编、内存、操作系统、并发、原子操作、语言运行时等实验实现。
2. **实验执行平台**：统一的实验注册、构建、运行、隔离、观测和验证流程。
3. **观测与分析工具**：汇编、ELF、系统调用、调度、内存和性能数据的统一归档与展示。
4. **原理与实现文档**：从语言规范到硬件实现的分层解释。
5. **验证体系**：单元测试、压力测试、Sanitizer、跨编译器和跨架构验证。
6. **可复现记录**：环境快照、构建参数、运行数据、原始证据和实验报告。

项目的完成标准不是“实现了多少个类”或“写了多少行代码”，而是能够针对一个底层机制，独立完成从提出问题、建立模型、实现机制、构建实验、采集证据、验证结果到解释差异的完整研究过程。

**最终定位：SysLab 是一个以代码实现为基础、以真实系统行为为研究对象、以实验验证为核心方法的计算机底层原理实践平台。