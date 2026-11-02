#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Risk classification from the experiment security model.
 * 实验安全模型中的风险等级。
 *
 * R3 and R4 are recorded and rejected on a daily host. A command-line flag
 * cannot promote them into the local runner.
 * R3 和 R4 会在日常主机上被记录并拒绝。命令行开关不能把它们提升为本地可运行。
 */
enum class RiskLevel {
  ReadOnly,
  Normal,
  ResourceIntensive,
  KernelPrivileged,
  BareMetal
};

/**
 * @brief Lifecycle state of one experiment invocation.
 * 一次实验调用的生命周期状态。
 *
 * Terminal states are Passed, Failed, Unsupported, and Interrupted.
 * A failed invocation is never rewritten as passed.
 * 终止状态是 Passed、Failed、Unsupported 和 Interrupted。失败的调用不会被改写成通过。
 */
enum class ExperimentState {
  Registered,
  Validating,
  EnvironmentCheck,
  Building,
  Built,
  Running,
  Observing,
  Verifying,
  Passed,
  Failed,
  Unsupported,
  Interrupted
};

/**
 * @brief Kind of evidence collected for a run.
 * 一次运行收集的证据种类。
 */
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
  StandardError,
  CompilerMetadata,
  ExitStatus,
  ElfMetadata
};

/**
 * @brief Result of one verification check.
 * 一条验证检查的结果。
 */
enum class VerificationStatus { Passed, Failed, Unsupported, Inconclusive };

/**
 * @brief Authority class for a statement in an experiment document.
 * 实验文档中一句话的依据类别。
 */
enum class Basis { Lang, Abi, Compiler, Os, Isa, Microarch, Observed };

/**
 * @brief Explicit collection status for a host fact.
 * 主机事实的显式采集状态。
 *
 * An empty value is not a status. Callers must read status before value.
 * 空值不是状态。调用方必须先读 status，再读 value。
 */
struct SampledField {
  enum class Status { Ok, Unavailable, Unsupported, Failed };

  Status status = Status::Unavailable;
  std::string value;
  std::string reason;
};

/**
 * @brief Identity and classification of an experiment.
 * 实验的身份和分类。
 */
struct ExperimentDescriptor {
  std::string id;
  std::string name;
  std::string module;
  std::string description;
  std::uint32_t schema_version = 1;
  RiskLevel risk = RiskLevel::Normal;
  std::vector<std::string> tags;
  std::vector<std::string> supported_platforms;
  std::vector<std::string> required_capabilities;
};

/**
 * @brief Host facts recorded with a run.
 * 一次运行记录的主机事实。
 *
 * Fields that cannot be collected stay in the Unavailable, Unsupported, or
 * Failed status. They are not left as empty strings.
 * 采不到的字段保持 Unavailable、Unsupported 或 Failed，不会留成空字符串。
 */
struct EnvironmentSnapshot {
  SampledField os_name;
  SampledField kernel_release;
  SampledField architecture;
  SampledField cpu_model;
  SampledField compiler_id;
  SampledField compiler_version;
  SampledField standard_library;
  SampledField libc_version;
  SampledField page_size;
  SampledField cpu_topology;
  /**
   * scaling_governor for the visible CPUs. One shared value when every readable
   * CPU reports the same governor. A per-CPU list when they differ. Unavailable
   * when sysfs does not expose the file.
   * 可见 CPU 的 scaling_governor。可读的 CPU 都相同时报一个共同值，不同时按 CPU 列出。sysfs 没有该文件时为 Unavailable。
   */
  SampledField cpufreq_governor;
  /**
   * The first three fields of /proc/loadavg.
   * /proc/loadavg 的前三个字段。
   */
  SampledField load_average;
  /**
   * CPU set of the runner process from sched_getaffinity. The runner does not
   * call sched_setaffinity, so this is the inherited mask, limited to
   * CPU_SETSIZE processors.
   * 运行进程经 sched_getaffinity 得到的 CPU 集合。运行器不调用 sched_setaffinity，因此这是继承的掩码，范围限于 CPU_SETSIZE 个处理器。
   */
  SampledField cpu_affinity;
  std::string environment_id;
};

/**
 * @brief Traceable compiler invocation for one experiment variant.
 * 一个实验变体可追溯的编译器调用。
 */
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
  std::string source_revision_reason;
  /**
   * True when git status --porcelain produced output. Meaningful only when
   * source_dirty_known is true. A false value with source_dirty_known false is
   * not a clean worktree.
   * git status --porcelain 有输出时为真。仅当 source_dirty_known 为真时才有意义。
   * source_dirty_known 为假时，false 不表示工作区干净。
   */
  bool source_dirty = false;
  bool source_dirty_known = false;
  std::string source_dirty_reason;
  /**
   * Lowercase SHA-256 of the manifest sources and the compiler dependency
   * prerequisites, or the literal unavailable when the set could not be hashed.
   * 清单源文件与编译器依赖前提的小写 SHA-256。无法计算时为字面量 unavailable。
   */
  std::string source_digest;
  std::string source_digest_reason;
  /**
   * Path, CMake build type, and SHA-256 of the linked libsyslab.a.
   * Each is the literal unavailable when this variant does not link that library.
   * 所链接 libsyslab.a 的路径、CMake 构建类型和 SHA-256。
   * 该变体不链接这个库时，三者都是字面量 unavailable。
   */
  std::string library_path;
  std::string library_build_type;
  std::string library_digest;
  std::string library_reason;
  bool cache_hit = false;
  std::string cache_build_id;
  std::string artifact_digest;
  std::string started_at;
  std::string completed_at;
  int exit_code = 1;
};

/**
 * @brief One execution of a built experiment.
 * 一次已构建实验的执行。
 */
struct RunRecord {
  std::string run_id;
  std::string experiment_id;
  std::string build_id;
  std::string environment_id;
  std::string started_at;
  std::string completed_at;
  std::uint64_t duration_ns = 0;
  int exit_code = 1;
  int term_signal = 0;
  bool timed_out = false;
  bool interrupted = false;
  bool output_limited = false;
  std::string termination = "completed";
  std::string stdout_artifact;
  std::string stderr_artifact;
};

/**
 * @brief Evidence associated with a run.
 * 与一次运行关联的证据。
 */
struct ObservationRecord {
  std::string observation_id;
  std::string run_id;
  ObservationKind kind;
  std::string tool_name;
  std::string tool_version;
  /**
   * Absolute path returned by the shared tool lookup. Empty when the
   * observation does not come from an external tool.
   * 统一工具查找返回的绝对路径。观察不来自外部工具时为空。
   */
  std::string tool_path;
  std::string artifact_path;
  std::string content_digest;
  std::string collected_at;
  bool complete = false;
  std::string limitation;
};

/**
 * @brief One manifest check evaluated against a run.
 * 对照一次运行评估的一条清单检查。
 */
struct VerificationResult {
  std::string check_id;
  std::string run_id;
  VerificationStatus status = VerificationStatus::Inconclusive;
  std::string expected;
  std::string actual;
  std::string explanation;
  std::vector<std::string> evidence_ids;
};

/**
 * @brief Returns the design name of a risk level.
 * 返回风险等级的设计名称。
 * @param risk Risk classification.
 * 风险等级。
 * @return Stable lowercase name.
 * 稳定的小写名称。
 */
const char* to_string(RiskLevel risk) noexcept;

/**
 * @brief Returns the design name of a lifecycle state.
 * 返回生命周期状态的设计名称。
 * @param state Lifecycle state.
 * 生命周期状态。
 * @return Uppercase state name.
 * 大写状态名。
 */
const char* to_string(ExperimentState state) noexcept;

/**
 * @brief Returns the record name of an observation kind.
 * 返回观察种类的记录名称。
 * @param kind Observation kind.
 * 观察种类。
 * @return Lowercase kind name.
 * 小写种类名。
 */
const char* to_string(ObservationKind kind) noexcept;

/**
 * @brief Returns the record name of a verification status.
 * 返回验证状态的记录名称。
 * @param status Verification status.
 * 验证状态。
 * @return Lowercase status name.
 * 小写状态名。
 */
const char* to_string(VerificationStatus status) noexcept;

/**
 * @brief Returns the authority label used in manifests and reports.
 * 返回清单和报告使用的依据标签。
 * @param basis Authority class.
 * 依据类别。
 * @return Uppercase label such as LANG or OS.
 * 大写标签，例如 LANG 或 OS。
 */
const char* to_string(Basis basis) noexcept;

/**
 * @brief Parses a risk token from a manifest.
 * 从清单解析风险记号。
 * @param text Manifest token.
 * 清单记号。
 * @return Risk level.
 * 风险等级。
 * @throws Error Validation when the token is unknown.
 * 记号未知时抛出 Error Validation。
 */
RiskLevel parse_risk(const std::string& text);

/**
 * @brief Parses an observation token from a manifest.
 * 从清单解析观察记号。
 * @param text Manifest token.
 * 清单记号。
 * @return Observation kind.
 * 观察种类。
 * @throws Error Validation when the token is unknown.
 * 记号未知时抛出 Error Validation。
 */
ObservationKind parse_observation_kind(const std::string& text);

/**
 * @brief Parses an authority token from a manifest.
 * 从清单解析依据记号。
 * @param text Manifest token.
 * 清单记号。
 * @return Authority class.
 * 依据类别。
 * @throws Error Validation when the token is unknown.
 * 记号未知时抛出 Error Validation。
 */
Basis parse_basis(const std::string& text);

}  // namespace syslab
