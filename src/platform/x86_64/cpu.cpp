#include "syslab/platform/cpu.hpp"

#include <fstream>
#include <string>

namespace syslab {
namespace {

std::size_t read_cache_line() {
  std::ifstream input("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
  if (!input) {
    return 0;
  }
  std::size_t value = 0;
  input >> value;
  if (!input || value == 0) {
    return 0;
  }
  return value;
}

}  // namespace

HostArchitecture host_architecture() {
  HostArchitecture architecture {};
#if defined(__x86_64__)
  architecture.name = "x86_64";
  architecture.instruction_pointer = "rip";
  architecture.stack_pointer = "rsp";
  architecture.integer_return_register = "rax";
  architecture.syscall_instruction = "syscall";
  architecture.memory_model = "TSO";
#elif defined(__aarch64__)
  architecture.name = "aarch64";
  architecture.instruction_pointer = "pc";
  architecture.stack_pointer = "sp";
  architecture.integer_return_register = "x0";
  architecture.syscall_instruction = "svc";
  architecture.memory_model = "weak";
#else
  architecture.name = "unknown";
  architecture.instruction_pointer = "unavailable";
  architecture.stack_pointer = "unavailable";
  architecture.integer_return_register = "unavailable";
  architecture.syscall_instruction = "unavailable";
  architecture.memory_model = "unavailable";
#endif
  architecture.pointer_bits = static_cast<int>(sizeof(void*) * 8);
  architecture.cache_line_bytes = read_cache_line();
  architecture.cache_line_known = architecture.cache_line_bytes != 0;
  return architecture;
}

}  // namespace syslab
