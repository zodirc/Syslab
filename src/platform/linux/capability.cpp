#include "syslab/platform/capability.hpp"

#include "syslab/core/fs.hpp"
#include "syslab/runner/process.hpp"

#include <dirent.h>

namespace syslab {
namespace {

std::string first_line(const std::string& text) {
  const std::size_t end = text.find('\n');
  if (end == std::string::npos) {
    return text;
  }
  return text.substr(0, end);
}

Capability probe_tool(const std::string& name, const std::vector<std::string>& version_args) {
  Capability capability;
  capability.name = name;
  CommandRequest request;
  request.argv.push_back(name);
  for (const std::string& arg : version_args) {
    request.argv.push_back(arg);
  }
  request.inherit_environment = true;
  request.timeout_ms = 3000;
  request.output_limit = 65536;
  const CommandResult result = run_command(request);
  const std::string version = first_line(result.stdout_text.empty() ? result.stderr_text : result.stdout_text);
  capability.version = version.empty() ? "unknown" : version;
  if (result.timed_out) {
    capability.status = "failed";
    capability.reason = "version probe timed out";
    return capability;
  }
  if (!result.spawn_error.empty() || result.exit_code != 0) {
    capability.status = "unavailable";
    capability.reason = result.spawn_error.empty() ? "probe exited " + std::to_string(result.exit_code)
                                                   : result.spawn_error;
    return capability;
  }
  capability.status = "ok";
  capability.reason.clear();
  return capability;
}

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
  capabilities.push_back(probe_tool("objdump", {"--version"}));
  capabilities.push_back(probe_tool("readelf", {"--version"}));
  capabilities.push_back(probe_tool("nm", {"--version"}));
  capabilities.push_back(probe_tool("strace", {"-V"}));
  capabilities.push_back(probe_tool("perf", {"--version"}));
  capabilities.push_back(probe_tool("gdb", {"--version"}));
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
