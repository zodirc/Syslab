#include "syslab/platform/capability.hpp"

#include "syslab/core/fs.hpp"
#include "syslab/runner/process.hpp"

#include <dirent.h>

namespace syslab {
namespace {

std::string first_line(const std::string& text) {
  const std::size_t end = text.find('\n');
  std::string line = end == std::string::npos ? text : text.substr(0, end);
  while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
    line.pop_back();
  }
  return line;
}

std::string trim_copy(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
    text.pop_back();
  }
  std::size_t begin = 0;
  while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\n' || text[begin] == '\r')) {
    ++begin;
  }
  return text.substr(begin);
}

Capability capability_from_tool(const std::string& name, const ResolvedTool& tool) {
  Capability capability;
  capability.name = name;
  capability.version = tool.version.empty() ? "unavailable" : tool.version;
  capability.reason = tool.reason;
  if (tool.available) {
    capability.status = "ok";
    capability.reason.clear();
  } else if (tool.path.empty()) {
    capability.status = "unavailable";
  } else {
    capability.status = "failed";
  }
  return capability;
}

}  // namespace

ResolvedTool resolve_tool(const std::string& name, const std::vector<std::string>& version_args) {
  ResolvedTool tool;
  tool.name = name;
  CommandRequest which_request;
  which_request.argv = {"which", name};
  which_request.inherit_environment = true;
  which_request.timeout_ms = 2000;
  which_request.output_limit = 4096;
  const CommandResult which = run_command(which_request);
  if (which.timed_out || !which.spawn_error.empty() || which.exit_code != 0) {
    tool.available = false;
    tool.version = "unavailable";
    tool.reason = !which.spawn_error.empty()
                      ? which.spawn_error
                      : (which.timed_out ? "which timed out" : "which did not resolve the command");
    return tool;
  }
  tool.path = first_line(which.stdout_text);
  if (tool.path.empty()) {
    tool.version = "unavailable";
    tool.reason = "which did not resolve the command";
    return tool;
  }
  CommandRequest version_request;
  version_request.argv.push_back(tool.path);
  for (const std::string& argument : version_args) {
    version_request.argv.push_back(argument);
  }
  version_request.inherit_environment = true;
  version_request.timeout_ms = 3000;
  version_request.output_limit = 65536;
  const CommandResult version = run_command(version_request);
  const std::string line = first_line(version.stdout_text.empty() ? version.stderr_text : version.stdout_text);
  tool.version = line.empty() ? "unavailable" : line;
  if (version.timed_out) {
    tool.reason = "version probe timed out";
    return tool;
  }
  if (!version.spawn_error.empty() || version.exit_code != 0) {
    tool.reason = version.spawn_error.empty() ? "probe exited " + std::to_string(version.exit_code) : version.spawn_error;
    return tool;
  }
  tool.available = true;
  return tool;
}

Capability probe_perf_events(const ResolvedTool& perf) {
  Capability capability;
  capability.name = "perf_events";
  if (!perf.available) {
    capability.status = "unavailable";
    capability.version = "unavailable";
    capability.reason = perf.reason.empty() ? "perf is unavailable" : perf.reason;
    return capability;
  }
  if (path_exists("/proc/sys/kernel/perf_event_paranoid")) {
    const std::string paranoid = trim_copy(read_file("/proc/sys/kernel/perf_event_paranoid"));
    capability.version = paranoid.empty() ? "unavailable" : paranoid;
  } else {
    capability.version = "unavailable";
  }
  CommandRequest request;
  request.argv = {perf.path, "stat", "-e", "cycles", "--", "/bin/true"};
  request.inherit_environment = true;
  request.timeout_ms = 3000;
  request.output_limit = 65536;
  const CommandResult result = run_command(request);
  if (result.timed_out || !result.spawn_error.empty() || result.exit_code != 0) {
    capability.status = "unavailable";
    const std::string detail = first_line(result.stderr_text.empty() ? result.stdout_text : result.stderr_text);
    if (!result.spawn_error.empty()) {
      capability.reason = result.spawn_error;
    } else if (result.timed_out) {
      capability.reason = "perf stat -e cycles -- /bin/true timed out";
    } else if (!detail.empty()) {
      capability.reason = detail;
    } else {
      capability.reason = "perf stat -e cycles -- /bin/true failed";
    }
    return capability;
  }
  capability.status = "ok";
  return capability;
}

namespace {

int count_numa_nodes() {
  DIR* handle = opendir("/sys/devices/system/node");
  if (handle == nullptr) {
    return -1;
  }
  int count = 0;
  while (dirent* entry = readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.compare(0, 4, "node") == 0 && name.size() > 4) {
      ++count;
    }
  }
  closedir(handle);
  return count;
}

}  // namespace

std::vector<Capability> probe_capabilities() {
  std::vector<Capability> capabilities;
  capabilities.push_back(capability_from_tool("objdump", resolve_tool("objdump", {"--version"})));
  capabilities.push_back(capability_from_tool("readelf", resolve_tool("readelf", {"--version"})));
  capabilities.push_back(capability_from_tool("nm", resolve_tool("nm", {"--version"})));
  capabilities.push_back(capability_from_tool("strace", resolve_tool("strace", {"-V"})));
  const ResolvedTool perf = resolve_tool("perf", {"--version"});
  capabilities.push_back(capability_from_tool("perf", perf));
  capabilities.push_back(probe_perf_events(perf));
  capabilities.push_back(capability_from_tool("gdb", resolve_tool("gdb", {"--version"})));
  Capability proc;
  proc.name = "procfs";
  if (is_directory("/proc/self")) {
    proc.status = "ok";
    proc.version = "/proc/self";
  } else {
    proc.status = "unavailable";
    proc.reason = "/proc/self is not a directory";
  }
  capabilities.push_back(proc);
  Capability numa;
  numa.name = "numa";
  const int nodes = count_numa_nodes();
  if (nodes < 0) {
    numa.status = "unavailable";
    numa.reason = "/sys/devices/system/node cannot be read";
  } else if (nodes < 2) {
    numa.status = "unsupported";
    numa.version = std::to_string(nodes);
    numa.reason = "fewer than two NUMA nodes are visible";
  } else {
    numa.status = "ok";
    numa.version = std::to_string(nodes);
  }
  capabilities.push_back(std::move(numa));
  return capabilities;
}

Json capabilities_to_json(const std::vector<Capability>& capabilities) {
  Json array = Json::array();
  for (const Capability& capability : capabilities) {
    Json item = Json::object();
    item.set("name", Json::string(capability.name));
    item.set("status", Json::string(capability.status));
    item.set("version", Json::string(capability.version));
    item.set("reason", Json::string(capability.reason));
    array.push(std::move(item));
  }
  return array;
}

const Capability* find_capability(const std::vector<Capability>& capabilities, const std::string& name) {
  for (const Capability& capability : capabilities) {
    if (capability.name == name) {
      return &capability;
    }
  }
  return nullptr;
}

}  // namespace syslab
