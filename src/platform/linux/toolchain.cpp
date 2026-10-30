#include "syslab/platform/toolchain.hpp"

#include "syslab/runner/process.hpp"

namespace syslab {
namespace {

std::string first_line(const std::string& text) {
  const std::size_t end = text.find('\n');
  return end == std::string::npos ? text : text.substr(0, end);
}

Toolchain probe(const std::string& id) {
  Toolchain toolchain;
  toolchain.id = id;
  CommandRequest version_request;
  version_request.argv = {id, "--version"};
  version_request.inherit_environment = true;
  version_request.timeout_ms = 3000;
  version_request.output_limit = 65536;
  const CommandResult version = run_command(version_request);
  if (!version.spawn_error.empty()) {
    toolchain.available = false;
    toolchain.reason = version.spawn_error;
    return toolchain;
  }
  toolchain.available = version.exit_code == 0 && !version.timed_out;
  toolchain.version = first_line(version.stdout_text.empty() ? version.stderr_text : version.stdout_text);
  if (toolchain.version.empty()) {
    toolchain.version = "unknown";
  }
  CommandRequest which_request;
  which_request.argv = {"which", id};
  which_request.inherit_environment = true;
  which_request.timeout_ms = 2000;
  const CommandResult which = run_command(which_request);
  if (which.exit_code == 0) {
    toolchain.path = first_line(which.stdout_text);
  } else {
    toolchain.path = id;
    toolchain.reason = "which did not resolve the command";
  }
  CommandRequest target_request;
  target_request.argv = {id, "-dumpmachine"};
  target_request.inherit_environment = true;
  target_request.timeout_ms = 3000;
  const CommandResult target = run_command(target_request);
  if (target.exit_code == 0) {
    toolchain.target = first_line(target.stdout_text);
  } else {
    toolchain.target = "unavailable";
    if (toolchain.reason.empty()) {
      toolchain.reason = "-dumpmachine failed";
    }
  }
  if (!toolchain.available && toolchain.reason.empty()) {
    toolchain.reason = "compiler version probe failed";
  }
  return toolchain;
}

}  // namespace

std::vector<Toolchain> list_toolchains() {
  return {probe("gcc"), probe("g++"), probe("clang"), probe("clang++")};
}

Toolchain inspect_toolchain(const std::string& id) {
  if (id != "gcc" && id != "g++" && id != "clang" && id != "clang++") {
    Toolchain toolchain;
    toolchain.id = id;
    toolchain.reason = "toolchain id must be gcc, g++, clang, or clang++";
    return toolchain;
  }
  return probe(id);
}

Json toolchains_to_json(const std::vector<Toolchain>& toolchains) {
  Json array = Json::array();
  for (const Toolchain& toolchain : toolchains) {
    Json item = Json::object();
    item.set("id", Json::string(toolchain.id));
    item.set("path", Json::string(toolchain.path));
    item.set("version", Json::string(toolchain.version));
    item.set("target", Json::string(toolchain.target));
    item.set("available", Json::boolean(toolchain.available));
    item.set("reason", Json::string(toolchain.reason));
    array.push(std::move(item));
  }
  return array;
}

}  // namespace syslab
