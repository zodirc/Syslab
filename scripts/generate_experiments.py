#!/usr/bin/env python3
"""Write experiment manifests, drivers, and principle notes."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPERIMENTS = []


def add(rel, identifier, name, module, description, code, purpose, checks, boundary,
        concepts, basis, observe=None, risk="normal", sources=None, extra=None,
        capabilities=None, timeout=20000):
    EXPERIMENTS.append({
        "rel": rel,
        "id": identifier,
        "name": name,
        "module": module,
        "description": description,
        "code": code,
        "purpose": purpose,
        "checks": checks,
        "boundary": boundary,
        "concepts": concepts,
        "basis": basis,
        "observe": observe or ["stdout", "stderr", "exit_status"],
        "risk": risk,
        "sources": sources,
        "extra": extra or {},
        "capabilities": capabilities or [],
        "timeout": timeout,
        "target": "exp_" + identifier.lower().replace("-", "_"),
    })


def manifest(exp):
    observe = "\n".join("    - " + item for item in exp["observe"])
    concepts = "\n".join("    - " + item for item in exp["concepts"])
    basis = "\n".join("    - " + item for item in exp["basis"])
    sources = ""
    if exp["sources"]:
        sources = "  sources:\n" + "\n".join("    - " + item for item in exp["sources"]) + "\n"
    capabilities = ""
    if exp["capabilities"]:
        capabilities = "  required_capabilities:\n" + "\n".join(
            "    - " + item for item in exp["capabilities"]) + "\n"
    return """schema_version: 1
experiment:
  id: {id}
  name: {name}
  module: {module}
  description: "{description}"
  risk: {risk}
  tags:
    - {tag}
{capabilities}platform:
  os: linux
  architectures:
    - x86_64
    - aarch64
build:
  target: {target}
  standard: c++20
{sources}  variants:
    - name: debug
      build_type: Debug
    - name: optimized
      build_type: Release
run:
  executable: {target}
  timeout_ms: {timeout}
  repetitions: 1
  arguments: []
observe:
{observe}
verify:
  checks:
    - name: exit
      type: exit_code
      expected: "0"
    - name: marker
      type: result_marker
      expected: pass
artifacts:
  preserve:
    - executable
    - build_log
    - run_log
knowledge:
  concepts:
{concepts}
  basis:
{basis}
""".format(
        id=exp["id"], name=exp["name"], module=exp["module"], description=exp["description"],
        risk=exp["risk"], tag=exp["module"].split(".")[0], capabilities=capabilities,
        target=exp["target"], sources=sources, timeout=exp["timeout"], observe=observe,
        concepts=concepts, basis=basis)


def readme(exp):
    return """# {id} {name}

## 实验目的

{purpose}

## 程序实际检查的内容

{checks}

## 不由本实验断言的内容

{boundary}

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
""".format(**exp)


def behavior(exp):
    return """# 预期行为

## 通过条件

{checks}

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

{boundary}
""".format(**exp)


# --- CPU ---

add(
    "experiments/cpu/integer-float", "CPU-001", "integer-float-representation", "cpu.representation",
    "Check two's complement and IEC 559 single-precision encoding.",
    r'''#include "experiments/common/check.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
int main() {
  const int negative = -1;
  const auto bytes = reinterpret_cast<const unsigned char*>(&negative);
  for (std::size_t index = 0; index < sizeof(negative); ++index) {
    if (bytes[index] != 0xff) return syslab_fail("twos-complement");
  }
  if (!std::numeric_limits<float>::is_iec559) return syslab_unsupported("float-not-iec559");
  float one = 1.0f;
  std::uint32_t bits = 0;
  std::memcpy(&bits, &one, sizeof(bits));
  syslab_metric("float_one_bits", bits);
  if (bits != 0x3f800000u) return syslab_fail("ieee754-single");
  syslab_note("LANG", "C++20 requires two's complement for signed integers");
  syslab_note("LANG", "1.0f bits are checked only because numeric_limits reports IEC 559");
  return syslab_pass();
}
''',
    "确认当前编译器下带符号整数的补码表示，以及 float 在声明为 IEC 559 时的 1.0 编码。",
    "int 类型的 -1 每个字节都是 0xff。若 numeric_limits<float>::is_iec559 为真，则 1.0f 的内存位型是 0x3f800000。",
    "本实验不把某一条浮点指令或寄存器分配写成必须出现的汇编。is_iec559 为假时结果是 unsupported，而不是改用推测编码。",
    ["twos-complement", "ieee754"], ["LANG"])

add(
    "experiments/cpu/call-return", "CPU-002", "call-and-return", "cpu.abi",
    "Call a function and check the returned sum.",
    r'''#include "experiments/common/check.hpp"
static long add_six(long a, long b, long c, long d, long e, long f) { return a + b + c + d + e + f; }
int main() {
  const long sum = add_six(1, 2, 3, 4, 5, 6);
  syslab_metric("sum", sum);
  if (sum != 21) return syslab_fail("sum");
  syslab_note("LANG", "the C++ abstract machine requires this function to return 21");
  syslab_note("ABI", "which registers carry the arguments is an ABI fact observed in the disassembly, not checked by opcode");
  return syslab_pass();
}
''',
    "用一次普通函数调用确认返回值，并把参数传递留给反汇编观察。",
    "add_six(1, 2, 3, 4, 5, 6) 的返回值等于 21。",
    "本实验不检查参数位于 rdi 还是 x0。那是 ABI 观察，由 CPU-004 的独立汇编文件做数值闭合，反汇编只保留现场。",
    ["calling-convention", "return-value"], ["LANG", "ABI"],
    ["disassembly", "stdout", "stderr", "exit_status"])

add(
    "experiments/cpu/stack-frame", "CPU-003", "stack-frame", "cpu.abi",
    "Keep an addressed local alive and record the optimization variant.",
    r'''#include "experiments/common/check.hpp"
#include <cstdint>
int main() {
  volatile int local = 7;
  const int* address = const_cast<const int*>(&local);
  if (address == nullptr || *address != 7) return syslab_fail("local");
  syslab_metric("local", *address);
#if defined(NDEBUG)
  syslab_note("COMPILER", "NDEBUG is defined for this variant");
#else
  syslab_note("COMPILER", "NDEBUG is not defined for this variant");
#endif
  syslab_note("COMPILER", "frame size differences between Debug and Release are disassembly observations");
  return syslab_pass();
}
''',
    "确认被取地址的局部对象在 Debug 和 Release 两种构建中仍然可读，并把栈帧差异留在反汇编里比较。",
    "volatile int 的值仍是 7，它的地址非空。",
    "帧指针是否保留、栈槽是否被合并，取决于编译器和优化级别，不是 C++ 标准规定的固定布局。比较两个变体的反汇编时要同时看 build.json。",
    ["stack-frame", "optimization"], ["LANG", "COMPILER"],
    ["disassembly", "compiler_metadata", "stdout", "stderr", "exit_status"])

ABI_SUM = r'''#if defined(__x86_64__)
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
'''

add(
    "experiments/abi/calling-convention", "CPU-004", "sysv-aarch64-abi-sum", "cpu.abi",
    "Sum six integer arguments with an architecture assembly stub.",
    r'''#include "experiments/common/check.hpp"
extern "C" long abi_sum6(long, long, long, long, long, long);
int main() {
  const long sum = abi_sum6(1, 2, 3, 4, 5, 6);
  syslab_metric("sum", sum);
  if (sum != 21) return syslab_fail("abi-sum");
#if defined(__x86_64__)
  syslab_note("ABI", "System V AMD64 uses rdi rsi rdx rcx r8 r9 for the first six integer arguments and rax for the result");
#elif defined(__aarch64__)
  syslab_note("ABI", "AArch64 PCS uses x0 through x5 for the first six integer arguments and x0 for the result");
#endif
  return syslab_pass();
}
''',
    "用目标 ABI 的前六个整数参数寄存器完成求和，用返回值闭合，而不是比对固定机器码。",
    "abi_sum6(1, 2, 3, 4, 5, 6) 返回 21。x86-64 汇编使用 rdi、rsi、rdx、rcx、r8、r9 和 rax。AArch64 汇编使用 x0 到 x5。",
    "其他编译器如何安排 C++ 函数的寄存器分配不由本文件规定。本实验只约束这个汇编入口。",
    ["sysv-amd64", "aarch64-pcs"], ["ABI", "ISA"],
    ["disassembly", "stdout", "stderr", "exit_status"],
    sources=["main.cpp", "abi_sum.S"], extra={"abi_sum.S": ABI_SUM})

add(
    "experiments/cpu/virtual-call", "CPU-005", "virtual-call", "cpu.object-model",
    "Dispatch a virtual call to the derived override.",
    r'''#include "experiments/common/check.hpp"
struct Base { virtual int value() const { return 1; } virtual ~Base() = default; };
struct Derived : Base { int value() const override { return 7; } };
int main() {
  Derived object;
  Base* pointer = &object;
  const int result = pointer->value();
  syslab_metric("virtual_result", result);
  if (result != 7) return syslab_fail("virtual-dispatch");
  syslab_note("LANG", "a virtual call uses the dynamic type");
  syslab_note("ABI", "the indirect call instruction and vtable layout are ABI observations in the disassembly");
  return syslab_pass();
}
''',
    "确认虚函数调用选择动态类型的覆盖函数。",
    "通过 Base* 调用 Derived::value 得到 7。",
    "虚表的内存布局、间接调用使用的具体寄存器属于 Itanium C++ ABI 和编译器实现，本实验只把反汇编作为观察。",
    ["virtual-dispatch"], ["LANG", "ABI"],
    ["disassembly", "stdout", "stderr", "exit_status"])

add(
    "experiments/cpu/elf-structure", "CPU-006", "elf-header", "cpu.elf",
    "Read the ELF identification of the running executable.",
    r'''#include "experiments/common/check.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
int main() {
  std::ifstream input("/proc/self/exe", std::ios::binary);
  if (!input) return syslab_unsupported("proc-self-exe");
  unsigned char identification[16] = {};
  input.read(reinterpret_cast<char*>(identification), 16);
  if (identification[0] != 0x7f || identification[1] != 'E' || identification[2] != 'L' || identification[3] != 'F') {
    return syslab_fail("elf-magic");
  }
  if (identification[4] != 2) return syslab_unsupported("not-elf64");
  syslab_metric("elf_class", identification[4]);
  syslab_metric("elf_data", identification[5]);
  syslab_note("ABI", "ELF identification is the System V ABI file format, not a C++ language rule");
  return syslab_pass();
}
''',
    "读取当前可执行文件的 ELF 标识，确认它是 ELF64。",
    "/proc/self/exe 的前四个字节是 0x7f、E、L、F，EI_CLASS 为 2。",
    "section、segment 和重定位的具体布局由运行器的 ELF 观察记录描述。本程序不把某一节名写成通过条件。字节序若不是当前实验假设，EI_DATA 只作为度量输出。",
    ["elf"], ["ABI"],
    ["elf", "stdout", "stderr", "exit_status"])

add(
    "experiments/cpu/dynamic-link", "CPU-007", "dynamic-link", "cpu.elf",
    "Check that the executable has a PT_INTERP segment.",
    r'''#include "experiments/common/check.hpp"
#include <cstdint>
#include <fstream>
#include <vector>
int main() {
  std::ifstream input("/proc/self/exe", std::ios::binary);
  if (!input) return syslab_unsupported("proc-self-exe");
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
  if (bytes.size() < 64 || bytes[0] != 0x7f) return syslab_fail("elf-magic");
  const unsigned char klass = bytes[4];
  const unsigned char data = bytes[5];
  if (klass != 2 || data != 1) return syslab_unsupported("not-elf64-le");
  auto read64 = [&](std::size_t offset) {
    std::uint64_t value = 0;
    for (int byte = 7; byte >= 0; --byte) value = (value << 8) | bytes[offset + static_cast<std::size_t>(byte)];
    return value;
  };
  auto read16 = [&](std::size_t offset) -> std::uint16_t {
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
  };
  const std::uint64_t phoff = read64(32);
  const std::uint16_t phentsize = read16(54);
  const std::uint16_t phnum = read16(56);
  bool interpreter = false;
  for (std::uint16_t index = 0; index < phnum; ++index) {
    const std::size_t entry = static_cast<std::size_t>(phoff) + static_cast<std::size_t>(index) * phentsize;
    if (entry + 4 > bytes.size()) return syslab_fail("phdr");
    const std::uint32_t type = static_cast<std::uint32_t>(bytes[entry] | (bytes[entry + 1] << 8) |
                                                         (bytes[entry + 2] << 16) | (bytes[entry + 3] << 24));
    if (type == 3) interpreter = true;
  }
  if (!interpreter) return syslab_unsupported("no-pt-interp");
  syslab_note("ABI", "PT_INTERP means the kernel invokes a dynamic loader for this executable");
  syslab_note("OBSERVED", "PLT and GOT contents are left in the disassembly and ELF observation");
  return syslab_pass();
}
''',
    "确认当前可执行文件包含 PT_INTERP，因此它走动态加载器，而不是静态入口。",
    "ELF64 小端文件的程序头中存在类型为 PT_INTERP（3）的段。",
    "没有解释器的静态链接文件得到 unsupported。PLT 槽的解析时机和 GOT 内容不作为数值断言。",
    ["dynamic-linking", "plt-got"], ["ABI", "OS"],
    ["elf", "disassembly", "stdout", "stderr", "exit_status"])

add(
    "experiments/compiler/optimization", "CPU-008", "constant-result", "compiler.optimization",
    "Check a value that remains 6 after legal optimization.",
    r'''#include "experiments/common/check.hpp"
static int sink(int value) { return value; }
static int constant_result() {
  const int left = 2;
  const int right = 3;
  if (left == 0) return sink(right);
  return left * right;
}
int main() {
  const int value = constant_result();
  syslab_metric("result", value);
  if (value != 6) return syslab_fail("result");
  syslab_note("LANG", "the abstract machine evaluates this function to 6");
  syslab_note("COMPILER", "constant folding and dead-code elimination are permitted and visible only by comparing disassembly");
  return syslab_pass();
}
''',
    "确认一个在抽象机中结果固定的函数返回 6，并说明优化是否发生要看反汇编。",
    "constant_result() 返回 6。",
    "编译器可以保留乘法，也可以折叠成立即数。两种代码都满足语言规则。本实验不因缺少某条指令而失败。",
    ["constant-folding", "dead-code"], ["LANG", "COMPILER"],
    ["disassembly", "compiler_metadata", "stdout", "stderr", "exit_status"])

add(
    "experiments/cpu/atomic-instructions", "CPU-009", "atomic-instruction-observation", "cpu.atomic",
    "Perform relaxed and sequentially consistent atomics and record their results.",
    r'''#include "experiments/common/check.hpp"
#include <atomic>
int main() {
  std::atomic<int> value{0};
  value.store(1, std::memory_order_relaxed);
  value.store(2, std::memory_order_seq_cst);
  const int added = value.fetch_add(3, std::memory_order_seq_cst);
  if (added != 2 || value.load(std::memory_order_seq_cst) != 5) return syslab_fail("atomic-result");
  syslab_metric("value", value.load(std::memory_order_relaxed));
  syslab_note("LANG", "the atomic modifications are visible to this thread in program order");
  syslab_note("ISA", "whether seq_cst became xchg, lock, or dmb is an instruction selection observed in the disassembly");
  return syslab_pass();
}
''',
    "在单线程里检查原子读改写的数值，并把不同 memory_order 的指令形式留在反汇编中。",
    "relaxed 存入 1，seq_cst 存入 2，fetch_add(3) 的返回值是 2，随后的值是 5。",
    "本实验不要求出现 lock 前缀、xchg 或 dmb。指令选择依赖编译器和 ISA。",
    ["memory-order", "rmw"], ["LANG", "ISA"],
    ["disassembly", "stdout", "stderr", "exit_status"])

add(
    "experiments/cpu/branch-pattern", "CPU-010", "branch-pattern", "cpu.microarch",
    "Compare predictable and data-dependent branches by checksum, and record durations.",
    r'''#include "experiments/common/check.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
int main() {
  constexpr int kSize = 200000;
  std::vector<int> values(kSize);
  for (int index = 0; index < kSize; ++index) values[static_cast<std::size_t>(index)] = index;
  auto accumulate = [&](bool mixed) {
    std::uint64_t sum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int repeat = 0; repeat < 20; ++repeat) {
      for (int index = 0; index < kSize; ++index) {
        const int value = values[static_cast<std::size_t>(index)];
        const bool taken = mixed ? ((value * 17) & 64) != 0 : value >= 0;
        if (taken) sum += static_cast<std::uint64_t>(value);
      }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return std::pair<std::uint64_t, long long>{sum, std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()};
  };
  const auto predictable = accumulate(false);
  const auto mixed = accumulate(true);
  syslab_metric("predictable_us", predictable.second);
  syslab_metric("mixed_us", mixed.second);
  if (predictable.first == 0 || mixed.first == 0) return syslab_fail("checksum");
  syslab_note("LANG", "both loops compute a defined sum");
  syslab_note("MICROARCH", "a shorter predictable loop is not required; branch-predictor behavior is not a language rule");
  syslab_note("OBSERVED", "hardware performance counters were not required for the pass condition");
  return syslab_pass();
}
''',
    "计算两种分支模式下的确定校验和，并记录耗时。通过条件不依赖哪一次更快。",
    "两种循环的校验和都不为 0。耗时以微秒度量写入 SYSLAB_METRIC。",
    "分支预测命中率、性能计数器事件名和谁更快都属于微架构观察。本主机若没有 perf，观察记录会标明未采样。",
    ["branch-prediction"], ["LANG", "MICROARCH", "OBSERVED"],
    ["performance_counter", "stdout", "stderr", "exit_status"], risk="resource_intensive", timeout=60000)

from experiments_rest import register_rest
register_rest(add)

def main():
    for exp in EXPERIMENTS:
        directory = ROOT / exp["rel"]
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "experiment.yaml").write_text(manifest(exp), encoding="utf-8")
        (directory / "main.cpp").write_text(exp["code"], encoding="utf-8")
        (directory / "README.md").write_text(readme(exp), encoding="utf-8")
        expected = directory / "expected"
        expected.mkdir(exist_ok=True)
        (expected / "behavior.md").write_text(behavior(exp), encoding="utf-8")
        for name, contents in exp["extra"].items():
            (directory / name).write_text(contents, encoding="utf-8")
    print("wrote", len(EXPERIMENTS), "experiments")

if __name__ == "__main__":
    main()
