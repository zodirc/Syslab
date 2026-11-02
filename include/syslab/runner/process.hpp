#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief Structured process request.
 * 结构化的进程请求。
 *
 * argv[0] is the executable path. The runner never joins arguments into a
 * shell command. When inherit_environment is false, the child receives a
 * fixed whitelist plus the explicit environment entries.
 * argv[0] 是可执行文件路径。运行器不会把参数拼成 shell 命令。inherit_environment 为假时，子进程只收到固定白名单加上显式环境项。
 */
struct CommandRequest {
  std::vector<std::string> argv;
  std::string work_directory;
  std::vector<std::pair<std::string, std::string>> environment;
  bool inherit_environment = false;
  std::uint64_t timeout_ms = 10000;
  std::uint64_t output_limit = 1048576;
  std::uint64_t memory_bytes = 0;
  std::uint64_t cpu_seconds = 0;
  std::string stdout_path;
  std::string stderr_path;
};

/**
 * @brief Result of one structured process execution.
 * 一次结构化进程执行的结果。
 *
 * termination is completed, signal, timed_out, output_limit, resource_limit,
 * interrupted, or spawn_error. A non-zero exit code uses completed when the
 * process exited by itself.
 * termination 为 completed、signal、timed_out、output_limit、resource_limit、interrupted 或 spawn_error。进程自行退出时，非零退出码仍记为 completed。
 */
struct CommandResult {
  int exit_code = 1;
  int term_signal = 0;
  bool timed_out = false;
  bool output_limited = false;
  bool interrupted = false;
  std::string termination = "spawn_error";
  std::string stdout_text;
  std::string stderr_text;
  std::uint64_t duration_ns = 0;
  std::string spawn_error;
  /**
   * Set when the parent cannot open stdout_path. The in-memory stdout_text
   * can still hold the pipe capture.
   * 父进程无法打开 stdout_path 时填写。内存中的 stdout_text 仍可以保留管道捕获。
   */
  std::string stdout_capture_error;
  /**
   * Set when the parent cannot open stderr_path. The in-memory stderr_text
   * can still hold the pipe capture.
   * 父进程无法打开 stderr_path 时填写。内存中的 stderr_text 仍可以保留管道捕获。
   */
  std::string stderr_capture_error;
};

/**
 * @brief Asks in-flight runs to stop. Safe to call from a signal handler.
 * 要求进行中的运行停止。可以从信号处理函数调用。
 */
void request_interrupt();

/**
 * @brief Reports whether request_interrupt has been called.
 * 报告是否已调用 request_interrupt。
 * @return True after an interrupt request.
 * 发出中断请求后为真。
 */
bool interrupt_requested();

/**
 * @brief Clears the interrupt flag. Used by tests and a fresh CLI invocation.
 * 清除中断标志。供测试和新的命令行调用使用。
 */
void clear_interrupt();

/**
 * @brief Runs a process with timeout, output, CPU, and address-space limits.
 * 在超时、输出、CPU 和地址空间限制下运行进程。
 *
 * The calling thread is the only thread that may invoke this function while
 * other threads exist, because the implementation uses fork. The child calls
 * only async-signal-safe operations before exec.
 * 存在其他线程时，只有调用线程可以调用本函数，因为实现使用 fork。子进程在 exec 之前只调用异步信号安全的操作。
 *
 * @param request Structured command.
 * 结构化命令。
 * @return Execution result. Spawn failures are reported in the result.
 * 执行结果。启动失败写在结果里。
 * @throws Error InvalidArgument when argv is empty or a limit is inconsistent.
 * argv 为空或限制互相矛盾时抛出 Error InvalidArgument。
 */
CommandResult run_command(const CommandRequest& request);

/**
 * @brief Builds the default child environment for an experiment.
 * 为实验构造默认的子进程环境。
 * @param extra Additional variables. They override whitelist entries with the same name.
 * 附加变量。同名项覆盖白名单。
 * @return Environment pairs.
 * 环境变量对。
 */
std::vector<std::pair<std::string, std::string>> sandbox_environment(
    const std::vector<std::pair<std::string, std::string>>& extra);

}  // namespace syslab
