#include "syslab/platform/environment.hpp"

#include "syslab/core/fs.hpp"
#include "syslab/core/time.hpp"

#include <sys/utsname.h>
#include <unistd.h>

#include <fstream>
#include <sstream>

namespace syslab {
namespace {

SampledField ok_field(std::string value) {
  SampledField field;
  field.status = SampledField::Status::Ok;
  field.value = std::move(value);
  return field;
}

SampledField failed_field(std::string reason) {
  SampledField field;
  field.status = SampledField::Status::Failed;
  field.reason = std::move(reason);
  return field;
}

SampledField unavailable_field(std::string reason) {
  SampledField field;
  field.status = SampledField::Status::Unavailable;
  field.reason = std::move(reason);
  return field;
}

std::string read_cpu_model() {
  std::ifstream input("/proc/cpuinfo");
  if (!input) {
    return {};
  }
  std::string line;
  while (std::getline(input, line)) {
    const std::string key = "model name";
    if (line.compare(0, key.size(), key) == 0) {
      const std::size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::size_t begin = colon + 1;
        while (begin < line.size() && line[begin] == ' ') {
          ++begin;
        }
        return line.substr(begin);
      }
    }
  }
  return {};
}

std::string read_topology() {
  const long online = sysconf(_SC_NPROCESSORS_ONLN);
  const long configured = sysconf(_SC_NPROCESSORS_CONF);
  if (online < 0 || configured < 0) {
    return {};
  }
  std::ostringstream out;
  out << "configured=" << configured << " online=" << online;
  return out.str();
}

Json field_json(const SampledField& field) {
  Json json = Json::object();
  const char* status = "unavailable";
  switch (field.status) {
    case SampledField::Status::Ok:
      status = "ok";
      break;
    case SampledField::Status::Unavailable:
      status = "unavailable";
      break;
    case SampledField::Status::Unsupported:
      status = "unsupported";
      break;
    case SampledField::Status::Failed:
      status = "failed";
      break;
  }
  json.set("status", Json::string(status));
  json.set("value", Json::string(field.value));
  json.set("reason", Json::string(field.reason));
  return json;
}

}  // namespace

EnvironmentSnapshot capture_environment(const std::string& compiler_path) {
  EnvironmentSnapshot snapshot;
  snapshot.environment_id = make_id("env");
  utsname system {};
  if (uname(&system) == 0) {
    snapshot.os_name = ok_field(system.sysname);
    snapshot.kernel_release = ok_field(system.release);
    snapshot.architecture = ok_field(system.machine);
  } else {
    snapshot.os_name = failed_field("uname failed");
    snapshot.kernel_release = failed_field("uname failed");
    snapshot.architecture = failed_field("uname failed");
  }
  const std::string model = read_cpu_model();
  if (model.empty()) {
    snapshot.cpu_model = unavailable_field("/proc/cpuinfo has no model name");
  } else {
    snapshot.cpu_model = ok_field(model);
  }
  const long page = sysconf(_SC_PAGESIZE);
  if (page > 0) {
    snapshot.page_size = ok_field(std::to_string(page));
  } else {
    snapshot.page_size = failed_field("sysconf(_SC_PAGESIZE) failed");
  }
  const std::string topology = read_topology();
  if (topology.empty()) {
    snapshot.cpu_topology = failed_field("processor count is unavailable");
  } else {
    snapshot.cpu_topology = ok_field(topology);
  }
  if (compiler_path.empty()) {
    snapshot.compiler_id = unavailable_field("no compiler was selected");
    snapshot.compiler_version = unavailable_field("no compiler was selected");
  } else {
    snapshot.compiler_id = ok_field(compiler_path);
    snapshot.compiler_version = unavailable_field("version is recorded by the toolchain probe");
  }
#if defined(_GLIBCXX_RELEASE)
  snapshot.standard_library = ok_field("libstdc++ " + std::to_string(_GLIBCXX_RELEASE));
#else
  snapshot.standard_library = unavailable_field("standard library release macro is absent");
#endif
#if defined(__GLIBC__)
  snapshot.libc_version = ok_field("glibc " + std::to_string(__GLIBC__) + "." + std::to_string(__GLIBC_MINOR__));
#else
  snapshot.libc_version = unavailable_field("glibc version macros are absent");
#endif
  return snapshot;
}

Json environment_to_json(const EnvironmentSnapshot& snapshot) {
  Json json = Json::object();
  json.set("environment_id", Json::string(snapshot.environment_id));
  json.set("os_name", field_json(snapshot.os_name));
  json.set("kernel_release", field_json(snapshot.kernel_release));
  json.set("architecture", field_json(snapshot.architecture));
  json.set("cpu_model", field_json(snapshot.cpu_model));
  json.set("compiler_id", field_json(snapshot.compiler_id));
  json.set("compiler_version", field_json(snapshot.compiler_version));
  json.set("standard_library", field_json(snapshot.standard_library));
  json.set("libc_version", field_json(snapshot.libc_version));
  json.set("page_size", field_json(snapshot.page_size));
  json.set("cpu_topology", field_json(snapshot.cpu_topology));
  return json;
}

}  // namespace syslab
