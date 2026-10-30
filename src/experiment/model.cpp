#include "syslab/experiment/model.hpp"

#include "syslab/core/error.hpp"

namespace syslab {

const char* to_string(RiskLevel risk) noexcept {
  switch (risk) {
    case RiskLevel::ReadOnly:
      return "readonly";
    case RiskLevel::Normal:
      return "normal";
    case RiskLevel::ResourceIntensive:
      return "resource_intensive";
    case RiskLevel::KernelPrivileged:
      return "kernel_privileged";
    case RiskLevel::BareMetal:
      return "bare_metal";
  }
  return "normal";
}

const char* to_string(ExperimentState state) noexcept {
  switch (state) {
    case ExperimentState::Registered:
      return "REGISTERED";
    case ExperimentState::Validating:
      return "VALIDATING";
    case ExperimentState::EnvironmentCheck:
      return "ENVIRONMENT_CHECK";
    case ExperimentState::Building:
      return "BUILDING";
    case ExperimentState::Built:
      return "BUILT";
    case ExperimentState::Running:
      return "RUNNING";
    case ExperimentState::Observing:
      return "OBSERVING";
    case ExperimentState::Verifying:
      return "VERIFYING";
    case ExperimentState::Passed:
      return "PASSED";
    case ExperimentState::Failed:
      return "FAILED";
    case ExperimentState::Unsupported:
      return "UNSUPPORTED";
    case ExperimentState::Interrupted:
      return "INTERRUPTED";
  }
  return "FAILED";
}

const char* to_string(ObservationKind kind) noexcept {
  switch (kind) {
    case ObservationKind::Disassembly:
      return "disassembly";
    case ObservationKind::SyscallTrace:
      return "syscall_trace";
    case ObservationKind::SchedulerTrace:
      return "scheduler_trace";
    case ObservationKind::PerformanceCounter:
      return "performance_counter";
    case ObservationKind::MemoryMap:
      return "memory_map";
    case ObservationKind::ThreadState:
      return "thread_state";
    case ObservationKind::AllocatorStatistics:
      return "allocator_statistics";
    case ObservationKind::SanitizerReport:
      return "sanitizer_report";
    case ObservationKind::DebuggerSnapshot:
      return "debugger_snapshot";
    case ObservationKind::StandardOutput:
      return "stdout";
    case ObservationKind::StandardError:
      return "stderr";
    case ObservationKind::CompilerMetadata:
      return "compiler_metadata";
    case ObservationKind::ExitStatus:
      return "exit_status";
    case ObservationKind::ElfMetadata:
      return "elf";
  }
  return "stdout";
}

const char* to_string(VerificationStatus status) noexcept {
  switch (status) {
    case VerificationStatus::Passed:
      return "passed";
    case VerificationStatus::Failed:
      return "failed";
    case VerificationStatus::Unsupported:
      return "unsupported";
    case VerificationStatus::Inconclusive:
      return "inconclusive";
  }
  return "inconclusive";
}

const char* to_string(Basis basis) noexcept {
  switch (basis) {
    case Basis::Lang:
      return "LANG";
    case Basis::Abi:
      return "ABI";
    case Basis::Compiler:
      return "COMPILER";
    case Basis::Os:
      return "OS";
    case Basis::Isa:
      return "ISA";
    case Basis::Microarch:
      return "MICROARCH";
    case Basis::Observed:
      return "OBSERVED";
  }
  return "OBSERVED";
}

RiskLevel parse_risk(const std::string& text) {
  if (text == "readonly" || text == "read_only" || text == "R0") {
    return RiskLevel::ReadOnly;
  }
  if (text == "normal" || text == "R1") {
    return RiskLevel::Normal;
  }
  if (text == "resource_intensive" || text == "R2") {
    return RiskLevel::ResourceIntensive;
  }
  if (text == "kernel_privileged" || text == "R3") {
    return RiskLevel::KernelPrivileged;
  }
  if (text == "bare_metal" || text == "R4") {
    return RiskLevel::BareMetal;
  }
  throw Error(Error::Code::Validation, "unknown risk level '" + text + "'");
}

ObservationKind parse_observation_kind(const std::string& text) {
  if (text == "disassembly") return ObservationKind::Disassembly;
  if (text == "syscall_trace" || text == "strace") return ObservationKind::SyscallTrace;
  if (text == "scheduler_trace") return ObservationKind::SchedulerTrace;
  if (text == "performance_counter" || text == "perf") return ObservationKind::PerformanceCounter;
  if (text == "memory_map") return ObservationKind::MemoryMap;
  if (text == "thread_state") return ObservationKind::ThreadState;
  if (text == "allocator_statistics") return ObservationKind::AllocatorStatistics;
  if (text == "sanitizer_report") return ObservationKind::SanitizerReport;
  if (text == "debugger_snapshot") return ObservationKind::DebuggerSnapshot;
  if (text == "stdout") return ObservationKind::StandardOutput;
  if (text == "stderr") return ObservationKind::StandardError;
  if (text == "compiler_metadata") return ObservationKind::CompilerMetadata;
  if (text == "exit_status") return ObservationKind::ExitStatus;
  if (text == "elf") return ObservationKind::ElfMetadata;
  throw Error(Error::Code::Validation, "unknown observation '" + text + "'");
}

Basis parse_basis(const std::string& text) {
  if (text == "LANG") return Basis::Lang;
  if (text == "ABI") return Basis::Abi;
  if (text == "COMPILER") return Basis::Compiler;
  if (text == "OS") return Basis::Os;
  if (text == "ISA") return Basis::Isa;
  if (text == "MICROARCH") return Basis::Microarch;
  if (text == "OBSERVED") return Basis::Observed;
  throw Error(Error::Code::Validation, "unknown basis '" + text + "'");
}

}  // namespace syslab
