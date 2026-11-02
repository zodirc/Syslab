#include "syslab/experiment/manifest.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/yaml.hpp"

#include <cctype>

namespace syslab {
namespace {

const Yaml* optional_key(const Yaml& node, const char* key) {
  if (node.kind() != Yaml::Kind::Map) {
    return nullptr;
  }
  return node.find(key);
}

std::string required_scalar(const Yaml& node, const char* key) {
  const Yaml& value = node.require(key);
  if (value.kind() != Yaml::Kind::Scalar || value.as_scalar().empty()) {
    throw Error(Error::Code::Validation, std::string(key) + " must be a non-empty scalar");
  }
  return value.as_scalar();
}

std::vector<std::string> string_list(const Yaml& node, const char* key, bool required) {
  const Yaml* value = optional_key(node, key);
  if (value == nullptr || value->kind() == Yaml::Kind::Null) {
    if (required) {
      throw Error(Error::Code::Validation, std::string(key) + " is required");
    }
    return {};
  }
  std::vector<std::string> items;
  for (const Yaml& entry : value->as_sequence()) {
    if (entry.kind() != Yaml::Kind::Scalar || entry.as_scalar().empty()) {
      throw Error(Error::Code::Validation, std::string(key) + " entries must be non-empty scalars");
    }
    items.push_back(entry.as_scalar());
  }
  if (required && items.empty()) {
    throw Error(Error::Code::Validation, std::string(key) + " must not be empty");
  }
  return items;
}

bool safe_relative_path(const std::string& path) {
  if (path.empty() || path.front() == '/') {
    return false;
  }
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t slash = path.find('/', start);
    const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    if (part.empty() || part == "." || part == "..") {
      return false;
    }
    if (part.find('\\') != std::string::npos) {
      return false;
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }
  return true;
}

bool valid_identifier(const std::string& id) {
  std::size_t dash = id.find('-');
  if (dash == std::string::npos || dash == 0 || dash + 1 >= id.size()) {
    return false;
  }
  for (std::size_t i = 0; i < dash; ++i) {
    if (!std::isupper(static_cast<unsigned char>(id[i]))) {
      return false;
    }
  }
  if (id.size() - dash - 1 != 3) {
    return false;
  }
  for (std::size_t i = dash + 1; i < id.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(id[i]))) {
      return false;
    }
  }
  return true;
}

bool valid_token(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  for (char character : token) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (!(std::isalnum(value) || character == '_' || character == '-' || character == '.')) {
      return false;
    }
  }
  return true;
}

std::uint64_t positive_integer(const Yaml& node, const char* key, std::uint64_t fallback, bool present_required) {
  const Yaml* value = optional_key(node, key);
  if (value == nullptr) {
    if (present_required) {
      throw Error(Error::Code::Validation, std::string(key) + " is required");
    }
    return fallback;
  }
  const long long parsed = value->as_integer();
  if (parsed <= 0) {
    throw Error(Error::Code::Validation, std::string(key) + " must be positive");
  }
  return static_cast<std::uint64_t>(parsed);
}

}  // namespace

Manifest load_manifest(const std::string& path) {
  const Yaml root = Yaml::parse(read_file(path), path);
  if (root.kind() != Yaml::Kind::Map) {
    throw Error(Error::Code::Validation, "manifest root must be a mapping");
  }
  Manifest manifest;
  manifest.manifest_path = path;
  manifest.directory = parent_path(path);
  const long long schema = root.require("schema_version").as_integer();
  if (schema != 1) {
    throw Error(Error::Code::Validation, "unsupported manifest schema_version");
  }
  manifest.experiment.schema_version = 1;

  const Yaml& experiment = root.require("experiment");
  manifest.experiment.id = required_scalar(experiment, "id");
  if (!valid_identifier(manifest.experiment.id)) {
    throw Error(Error::Code::Validation, "experiment id must look like AT-003");
  }
  manifest.experiment.name = required_scalar(experiment, "name");
  if (!valid_token(manifest.experiment.name)) {
    throw Error(Error::Code::Validation, "experiment name must be a token");
  }
  manifest.experiment.module = required_scalar(experiment, "module");
  manifest.experiment.description = required_scalar(experiment, "description");
  manifest.experiment.risk = parse_risk(required_scalar(experiment, "risk"));
  manifest.experiment.tags = string_list(experiment, "tags", false);
  manifest.experiment.required_capabilities = string_list(experiment, "required_capabilities", false);

  const Yaml& platform = root.require("platform");
  manifest.os = required_scalar(platform, "os");
  if (manifest.os != "linux") {
    throw Error(Error::Code::Validation, "platform.os must be linux");
  }
  manifest.architectures = string_list(platform, "architectures", true);
  for (const std::string& architecture : manifest.architectures) {
    if (architecture != "x86_64" && architecture != "aarch64") {
      throw Error(Error::Code::Validation, "unsupported architecture " + architecture);
    }
  }
  manifest.experiment.supported_platforms = manifest.architectures;

  const Yaml& build = root.require("build");
  manifest.target = required_scalar(build, "target");
  if (!valid_token(manifest.target)) {
    throw Error(Error::Code::Validation, "build.target must be a token");
  }
  manifest.standard = required_scalar(build, "standard");
  if (manifest.standard != "c++20" && manifest.standard != "c++17" && manifest.standard != "c17") {
    throw Error(Error::Code::Validation, "unsupported language standard");
  }
  manifest.sources = string_list(build, "sources", false);
  if (manifest.sources.empty()) {
    manifest.sources.push_back("main.cpp");
  }
  for (const std::string& source : manifest.sources) {
    if (!safe_relative_path(source)) {
      throw Error(Error::Code::Validation, "source path escapes the experiment directory: " + source);
    }
    if (!path_exists(join_path(manifest.directory, source))) {
      throw Error(Error::Code::Validation, "source does not exist: " + source);
    }
  }
  if (optional_key(build, "link") == nullptr) {
    manifest.link_libraries.push_back("syslab");
  } else {
    manifest.link_libraries = string_list(build, "link", false);
  }
  const Yaml& variants = build.require("variants");
  for (const Yaml& variant_node : variants.as_sequence()) {
    VariantSpec variant;
    variant.name = required_scalar(variant_node, "name");
    variant.build_type = required_scalar(variant_node, "build_type");
    if (variant.build_type != "Debug" && variant.build_type != "Release" &&
        variant.build_type != "RelWithDebInfo") {
      throw Error(Error::Code::Validation, "unknown build_type " + variant.build_type);
    }
    variant.flags = string_list(variant_node, "flags", false);
    for (const VariantSpec& existing : manifest.variants) {
      if (existing.name == variant.name) {
        throw Error(Error::Code::Validation, "duplicate variant " + variant.name);
      }
    }
    manifest.variants.push_back(std::move(variant));
  }
  if (manifest.variants.empty()) {
    throw Error(Error::Code::Validation, "build.variants must not be empty");
  }

  const Yaml& run = root.require("run");
  const Yaml* executable = optional_key(run, "executable");
  manifest.executable = executable == nullptr ? manifest.target : executable->as_scalar();
  if (!valid_token(manifest.executable)) {
    throw Error(Error::Code::Validation, "run.executable must be a token");
  }
  manifest.limits.timeout_ms = positive_integer(run, "timeout_ms", 10000, false);
  if (manifest.limits.timeout_ms > 600000) {
    throw Error(Error::Code::Validation, "timeout_ms exceeds 600000");
  }
  const Yaml* repetitions = optional_key(run, "repetitions");
  if (repetitions != nullptr) {
    const long long value = repetitions->as_integer();
    if (value < 1 || value > 100000) {
      throw Error(Error::Code::Validation, "repetitions must be in [1, 100000]");
    }
    manifest.repetitions = static_cast<int>(value);
  }
  manifest.arguments = string_list(run, "arguments", false);
  if (const Yaml* limits = optional_key(run, "limits")) {
    manifest.limits.output_bytes = positive_integer(*limits, "output_bytes", manifest.limits.output_bytes, false);
    if (const Yaml* memory = limits->find("memory_bytes")) {
      const long long value = memory->as_integer();
      if (value < 0) {
        throw Error(Error::Code::Validation, "memory_bytes must not be negative");
      }
      manifest.limits.memory_bytes = static_cast<std::uint64_t>(value);
    }
    if (const Yaml* cpu = limits->find("cpu_seconds")) {
      const long long value = cpu->as_integer();
      if (value < 0) {
        throw Error(Error::Code::Validation, "cpu_seconds must not be negative");
      }
      manifest.limits.cpu_seconds = static_cast<std::uint64_t>(value);
    }
    if (const Yaml* threads = limits->find("max_threads")) {
      manifest.limits.max_threads = positive_integer(*limits, "max_threads", 64, false);
    }
  }

  manifest.observe = string_list(root, "observe", false);
  if (manifest.observe.empty()) {
    manifest.observe = {"stdout", "stderr", "exit_status"};
  }
  bool wants_performance_counter = false;
  for (const std::string& observation : manifest.observe) {
    if (parse_observation_kind(observation) == ObservationKind::PerformanceCounter) {
      wants_performance_counter = true;
    }
  }
  manifest.perf_events = string_list(root, "perf_events", false);
  if (wants_performance_counter && manifest.perf_events.empty()) {
    throw Error(Error::Code::Validation, "performance_counter requires perf_events");
  }
  for (const std::string& event : manifest.perf_events) {
    if (event.empty()) {
      throw Error(Error::Code::Validation, "perf event name is empty");
    }
    for (char character : event) {
      const bool ok = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') || character == '_' || character == '.' ||
                      character == ':' || character == '/' || character == '-';
      if (!ok) {
        throw Error(Error::Code::Validation, "perf event name contains an unsupported character: " + event);
      }
    }
  }

  const Yaml& verify = root.require("verify");
  const Yaml& checks = verify.require("checks");
  for (const Yaml& check_node : checks.as_sequence()) {
    CheckSpec check;
    check.name = required_scalar(check_node, "name");
    check.type = required_scalar(check_node, "type");
    check.expected = required_scalar(check_node, "expected");
    if (check.type != "exit_code" && check.type != "stdout_contains" && check.type != "stderr_contains" &&
        check.type != "result_marker") {
      throw Error(Error::Code::Validation, "unknown check type " + check.type);
    }
    manifest.checks.push_back(std::move(check));
  }
  if (manifest.checks.empty()) {
    throw Error(Error::Code::Validation, "verify.checks must not be empty");
  }

  manifest.preserve = string_list(root.require("artifacts"), "preserve", false);
  if (const Yaml* knowledge = optional_key(root, "knowledge")) {
    manifest.prerequisites = string_list(*knowledge, "prerequisites", false);
    manifest.concepts = string_list(*knowledge, "concepts", false);
    manifest.basis = string_list(*knowledge, "basis", false);
    for (const std::string& basis : manifest.basis) {
      parse_basis(basis);
    }
    for (const std::string& prerequisite : manifest.prerequisites) {
      if (!valid_identifier(prerequisite)) {
        throw Error(Error::Code::Validation, "prerequisite is not an experiment id: " + prerequisite);
      }
    }
  }
  return manifest;
}

const VariantSpec& find_variant(const Manifest& manifest, const std::string& name) {
  if (name.empty()) {
    return manifest.variants.front();
  }
  for (const VariantSpec& variant : manifest.variants) {
    if (variant.name == name) {
      return variant;
    }
  }
  throw Error(Error::Code::NotFound, "unknown variant " + name);
}

Json manifest_to_json(const Manifest& manifest) {
  Json json = Json::object();
  json.set("id", Json::string(manifest.experiment.id));
  json.set("name", Json::string(manifest.experiment.name));
  json.set("module", Json::string(manifest.experiment.module));
  json.set("description", Json::string(manifest.experiment.description));
  json.set("risk", Json::string(to_string(manifest.experiment.risk)));
  json.set("schema_version", Json::integer(manifest.experiment.schema_version));
  Json tags = Json::array();
  for (const std::string& tag : manifest.experiment.tags) {
    tags.push(Json::string(tag));
  }
  json.set("tags", std::move(tags));
  Json architectures = Json::array();
  for (const std::string& architecture : manifest.architectures) {
    architectures.push(Json::string(architecture));
  }
  json.set("architectures", std::move(architectures));
  Json capabilities = Json::array();
  for (const std::string& capability : manifest.experiment.required_capabilities) {
    capabilities.push(Json::string(capability));
  }
  json.set("required_capabilities", std::move(capabilities));
  json.set("directory", Json::string(manifest.directory));
  json.set("target", Json::string(manifest.target));
  json.set("standard", Json::string(manifest.standard));
  Json variants = Json::array();
  for (const VariantSpec& variant : manifest.variants) {
    Json item = Json::object();
    item.set("name", Json::string(variant.name));
    item.set("build_type", Json::string(variant.build_type));
    variants.push(std::move(item));
  }
  json.set("variants", std::move(variants));
  Json prerequisites = Json::array();
  for (const std::string& prerequisite : manifest.prerequisites) {
    prerequisites.push(Json::string(prerequisite));
  }
  json.set("prerequisites", std::move(prerequisites));
  Json concepts = Json::array();
  for (const std::string& concept_name : manifest.concepts) {
    concepts.push(Json::string(concept_name));
  }
  json.set("concepts", std::move(concepts));
  Json perf_events = Json::array();
  for (const std::string& event : manifest.perf_events) {
    perf_events.push(Json::string(event));
  }
  json.set("perf_events", std::move(perf_events));
  Json basis = Json::array();
  for (const std::string& item : manifest.basis) {
    basis.push(Json::string(item));
  }
  json.set("basis", std::move(basis));
  return json;
}

}  // namespace syslab
