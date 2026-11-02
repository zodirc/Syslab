#include "syslab/runner/process.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/time.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <vector>

extern char** environ;

namespace syslab {
namespace {

std::atomic<int> g_interrupt {0};

struct Pipe {
  int read_fd = -1;
  int write_fd = -1;
  ~Pipe() {
    if (read_fd >= 0) {
      close(read_fd);
    }
    if (write_fd >= 0) {
      close(write_fd);
    }
  }
  Pipe() = default;
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
};

bool write_all(int fd, const char* data, std::size_t size) {
  std::size_t written = 0;
  while (written < size) {
    const ssize_t count = ::write(fd, data + written, size - written);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    written += static_cast<std::size_t>(count);
  }
  return true;
}

void append_output(CommandResult* result, bool is_stdout, const char* data, std::size_t size,
                   std::uint64_t* total, std::uint64_t limit, int* file_fd) {
  *total += size;
  if (*total > limit) {
    result->output_limited = true;
  }
  const std::uint64_t already = *total - size;
  if (already < limit) {
    const std::size_t keep = static_cast<std::size_t>(std::min<std::uint64_t>(size, limit - already));
    if (is_stdout) {
      result->stdout_text.append(data, keep);
    } else {
      result->stderr_text.append(data, keep);
    }
    if (file_fd != nullptr && *file_fd >= 0) {
      write_all(*file_fd, data, keep);
    }
  }
}

void set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

// Closes inherited descriptors before exec. pipe2(O_CLOEXEC) covers the two
// pipes at exec; this loop also drops descriptors the parent left open.
// Kernel 4.18 has no close_range, so the upper bound is sysconf(_SC_OPEN_MAX).
// On this host that value is 262144 and the loop costs about 145 ms.
void close_inherited_descriptors() {
  long open_max = ::sysconf(_SC_OPEN_MAX);
  if (open_max < 4) {
    open_max = 256;
  }
  for (int fd = 3; fd < open_max; ++fd) {
    close(fd);
  }
}

}  // namespace

void request_interrupt() { g_interrupt.store(1, std::memory_order_release); }

bool interrupt_requested() { return g_interrupt.load(std::memory_order_acquire) != 0; }

void clear_interrupt() { g_interrupt.store(0, std::memory_order_release); }

std::vector<std::pair<std::string, std::string>> sandbox_environment(
    const std::vector<std::pair<std::string, std::string>>& extra) {
  std::vector<std::pair<std::string, std::string>> environment;
  const char* keys[] = {"PATH", "HOME", "LANG", "LC_ALL", "USER", "TZ", "LD_LIBRARY_PATH"};
  for (const char* key : keys) {
    if (const char* value = std::getenv(key)) {
      environment.emplace_back(key, value);
    }
  }
  for (const auto& item : extra) {
    bool replaced = false;
    for (auto& existing : environment) {
      if (existing.first == item.first) {
        existing.second = item.second;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      environment.push_back(item);
    }
  }
  return environment;
}

CommandResult run_command(const CommandRequest& request) {
  if (request.argv.empty() || request.argv.front().empty()) {
    throw Error(Error::Code::InvalidArgument, "command argv must contain an executable");
  }
  if (request.timeout_ms == 0) {
    throw Error(Error::Code::InvalidArgument, "timeout_ms must be positive");
  }
  CommandResult result;
  Pipe output;
  Pipe error;
  int output_fds[2];
  int error_fds[2];
  if (pipe2(output_fds, O_CLOEXEC) != 0 || pipe2(error_fds, O_CLOEXEC) != 0) {
    result.spawn_error = std::string("pipe failed: ") + std::strerror(errno);
    result.termination = "spawn_error";
    return result;
  }
  output.read_fd = output_fds[0];
  output.write_fd = output_fds[1];
  error.read_fd = error_fds[0];
  error.write_fd = error_fds[1];

  std::vector<std::string> argv_storage = request.argv;
  std::vector<char*> argv_ptrs;
  argv_ptrs.reserve(argv_storage.size() + 1);
  for (std::string& argument : argv_storage) {
    argv_ptrs.push_back(argument.data());
  }
  argv_ptrs.push_back(nullptr);

  std::vector<std::string> env_storage;
  std::vector<char*> env_ptrs;
  if (!request.inherit_environment) {
    for (const auto& item : request.environment) {
      env_storage.push_back(item.first + "=" + item.second);
    }
    for (std::string& item : env_storage) {
      env_ptrs.push_back(item.data());
    }
    env_ptrs.push_back(nullptr);
  }

  const std::uint64_t started = steady_nanoseconds();
  const pid_t pid = fork();
  if (pid < 0) {
    result.spawn_error = std::string("fork failed: ") + std::strerror(errno);
    result.termination = "spawn_error";
    result.duration_ns = steady_nanoseconds() - started;
    return result;
  }
  if (pid == 0) {
    setpgid(0, 0);
    if (dup2(output.write_fd, STDOUT_FILENO) < 0 || dup2(error.write_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    close_inherited_descriptors();
    if (!request.work_directory.empty() && chdir(request.work_directory.c_str()) != 0) {
      _exit(126);
    }
    if (request.memory_bytes > 0) {
      rlimit limit {};
      limit.rlim_cur = static_cast<rlim_t>(request.memory_bytes);
      limit.rlim_max = static_cast<rlim_t>(request.memory_bytes);
      setrlimit(RLIMIT_AS, &limit);
    }
    if (request.cpu_seconds > 0) {
      rlimit limit {};
      limit.rlim_cur = static_cast<rlim_t>(request.cpu_seconds);
      limit.rlim_max = static_cast<rlim_t>(request.cpu_seconds);
      setrlimit(RLIMIT_CPU, &limit);
    }
    if (request.inherit_environment) {
      execvp(argv_ptrs[0], argv_ptrs.data());
    } else {
      execve(argv_ptrs[0], argv_ptrs.data(), env_ptrs.data());
    }
    _exit(127);
  }

  setpgid(pid, pid);
  close(output.write_fd);
  output.write_fd = -1;
  close(error.write_fd);
  error.write_fd = -1;
  set_nonblocking(output.read_fd);
  set_nonblocking(error.read_fd);

  int stdout_file = -1;
  int stderr_file = -1;
  if (!request.stdout_path.empty()) {
    stdout_file = open(request.stdout_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (stdout_file < 0) {
      result.stdout_capture_error = std::string("cannot open stdout capture: ") + std::strerror(errno);
    }
  }
  if (!request.stderr_path.empty()) {
    stderr_file = open(request.stderr_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (stderr_file < 0) {
      result.stderr_capture_error = std::string("cannot open stderr capture: ") + std::strerror(errno);
    }
  }

  std::uint64_t produced = 0;
  bool stdout_open = true;
  bool stderr_open = true;
  bool killed = false;
  bool reaped = false;
  bool abandoned = false;
  int kill_stage = 0;
  int reaped_status = 0;
  std::uint64_t kill_at = 0;
  const std::uint64_t deadline = started + request.timeout_ms * 1000000ull;

  while (stdout_open || stderr_open || !reaped) {
    pollfd fds[2];
    int count = 0;
    int stdout_index = -1;
    int stderr_index = -1;
    if (stdout_open) {
      stdout_index = count;
      fds[count].fd = output.read_fd;
      fds[count].events = POLLIN;
      fds[count].revents = 0;
      ++count;
    }
    if (stderr_open) {
      stderr_index = count;
      fds[count].fd = error.read_fd;
      fds[count].events = POLLIN;
      fds[count].revents = 0;
      ++count;
    }
    const int polled = poll(fds, static_cast<nfds_t>(count), 50);
    if (polled > 0) {
      auto drain = [&](int index, int fd, bool* open_flag, bool is_stdout) {
        if (index < 0) {
          return;
        }
        if ((fds[index].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
          return;
        }
        char buffer[4096];
        while (true) {
          const ssize_t got = ::read(fd, buffer, sizeof(buffer));
          if (got > 0) {
            append_output(&result, is_stdout, buffer, static_cast<std::size_t>(got), &produced,
                          request.output_limit, is_stdout ? &stdout_file : &stderr_file);
          } else if (got == 0) {
            *open_flag = false;
            break;
          } else if (errno == EINTR) {
            continue;
          } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          } else {
            *open_flag = false;
            break;
          }
        }
      };
      drain(stdout_index, output.read_fd, &stdout_open, true);
      drain(stderr_index, error.read_fd, &stderr_open, false);
    }
    const std::uint64_t now = steady_nanoseconds();
    if (!killed && (now >= deadline || result.output_limited || interrupt_requested())) {
      kill(-pid, SIGTERM);
      killed = true;
      kill_stage = 1;
      kill_at = now;
      if (now >= deadline) {
        result.timed_out = true;
      }
      if (interrupt_requested()) {
        result.interrupted = true;
      }
    }
    if (killed && kill_stage == 1 && steady_nanoseconds() - kill_at > 200000000ull) {
      kill(-pid, SIGKILL);
      kill_stage = 2;
      kill_at = steady_nanoseconds();
    }
    if (killed && kill_stage == 2 && steady_nanoseconds() - kill_at > 200000000ull) {
      if (output.read_fd >= 0) {
        close(output.read_fd);
        output.read_fd = -1;
      }
      if (error.read_fd >= 0) {
        close(error.read_fd);
        error.read_fd = -1;
      }
      stdout_open = false;
      stderr_open = false;
      abandoned = true;
      result.duration_ns = steady_nanoseconds() - started;
      if (!reaped) {
        const pid_t waited = waitpid(pid, &reaped_status, WNOHANG);
        if (waited == pid) {
          reaped = true;
        }
      }
      if (!reaped) {
        if (result.output_limited) {
          result.termination = "output_limit";
        } else if (result.interrupted) {
          result.termination = "interrupted";
        } else {
          result.timed_out = true;
          result.termination = "timed_out";
        }
      }
      break;
    }
    if (!reaped) {
      const pid_t waited = waitpid(pid, &reaped_status, WNOHANG);
      if (waited == pid) {
        reaped = true;
        result.duration_ns = steady_nanoseconds() - started;
      }
    }
    if (reaped && !stdout_open && !stderr_open) {
      break;
    }
  }

  if (!reaped && !abandoned) {
    if (waitpid(pid, &reaped_status, 0) == pid) {
      reaped = true;
      result.duration_ns = steady_nanoseconds() - started;
    }
  }
  if (reaped) {
    if (WIFEXITED(reaped_status)) {
      result.exit_code = WEXITSTATUS(reaped_status);
      result.termination = "completed";
    } else if (WIFSIGNALED(reaped_status)) {
      result.term_signal = WTERMSIG(reaped_status);
      result.exit_code = 128 + result.term_signal;
      if (result.term_signal == SIGXCPU) {
        result.termination = "resource_limit";
      } else if (result.timed_out) {
        result.termination = "timed_out";
      } else if (result.interrupted) {
        result.termination = "interrupted";
      } else if (result.output_limited) {
        result.termination = "output_limit";
      } else {
        result.termination = "signal";
      }
    }
  }
  if (result.output_limited && result.termination == "completed") {
    result.termination = "output_limit";
  }
  if (result.timed_out && result.termination == "completed") {
    result.termination = "timed_out";
  }
  if (stdout_file >= 0) {
    close(stdout_file);
  }
  if (stderr_file >= 0) {
    close(stderr_file);
  }
  return result;
}

}  // namespace syslab
