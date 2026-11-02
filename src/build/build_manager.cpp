#include "syslab/build/build_manager.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/time.hpp"
#include "syslab/runner/process.hpp"

#include <algorithm>
#include <cstdint>
#include <unistd.h>

namespace syslab {
namespace {

std::string first_line(const std::string& text) {
  const std::size_t end = text.find('\n');
  return end == std::string::npos ? text : text.substr(0, end);
}

std::vector<std::string> optimization_flags(const std::string& build_type) {
  if (build_type == "Debug") {
    return {"-O0", "-g", "-fno-omit-frame-pointer"};
  }
  if (build_type == "RelWithDebInfo") {
    return {"-O2", "-g", "-DNDEBUG"};
  }
  return {"-O2", "-DNDEBUG"};
}

std::string probe_text(const std::string& compiler, const std::string& flag) {
  CommandRequest request;
  request.argv = {compiler, flag};
  request.inherit_environment = true;
  request.timeout_ms = 5000;
  const CommandResult result = run_command(request);
  if (result.exit_code != 0) {
    return "unavailable";
  }
  const std::string line = first_line(result.stdout_text.empty() ? result.stderr_text : result.stdout_text);
  return line.empty() ? "unavailable" : line;
}

std::string source_revision(const std::string& root, std::string* reason) {
  CommandRequest request;
  request.argv = {"git", "-C", root, "rev-parse", "HEAD"};
  request.inherit_environment = true;
  request.timeout_ms = 3000;
  const CommandResult result = run_command(request);
  if (result.exit_code != 0 || result.stdout_text.empty()) {
    *reason = result.spawn_error.empty() ? "git rev-parse failed" : result.spawn_error;
    return "unavailable";
  }
  std::string revision = first_line(result.stdout_text);
  if (revision.empty()) {
    *reason = "git returned an empty revision";
    return "unavailable";
  }
  return revision;
}

void read_source_dirty(const std::string& root, BuildRecord* record) {
  CommandRequest request;
  request.argv = {"git", "-C", root, "status", "--porcelain"};
  request.inherit_environment = true;
  request.timeout_ms = 10000;
  const CommandResult result = run_command(request);
  if (result.exit_code != 0) {
    record->source_dirty_known = false;
    record->source_dirty = false;
    record->source_dirty_reason = result.spawn_error.empty() ? "git status --porcelain failed" : result.spawn_error;
    return;
  }
  bool dirty = false;
  for (char character : result.stdout_text) {
    if (character != ' ' && character != '\n' && character != '\r' && character != '\t') {
      dirty = true;
      break;
    }
  }
  record->source_dirty_known = true;
  record->source_dirty = dirty;
  record->source_dirty_reason.clear();
}

void append_u64(std::string* payload, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    payload->push_back(static_cast<char>((value >> shift) & 0xff));
  }
}

Json string_array(const std::vector<std::string>& values) {
  Json array = Json::array();
  for (const std::string& value : values) {
    array.push(Json::string(value));
  }
  return array;
}

Json build_record_json(const BuildRecord& record, bool success, const std::string& message) {
  Json json = Json::object();
  json.set("build_id", Json::string(record.build_id));
  json.set("experiment_id", Json::string(record.experiment_id));
  json.set("compiler_path", Json::string(record.compiler_path));
  json.set("compiler_version", Json::string(record.compiler_version));
  json.set("target_triple", Json::string(record.target_triple));
  json.set("language_standard", Json::string(record.language_standard));
  json.set("compile_arguments", string_array(record.compile_arguments));
  json.set("link_arguments", string_array(record.link_arguments));
  json.set("source_revision", Json::string(record.source_revision));
  json.set("source_revision_reason", Json::string(record.source_revision_reason));
  Json dirty = Json::object();
  if (record.source_dirty_known) {
    dirty.set("status", Json::string("ok"));
    dirty.set("value", Json::boolean(record.source_dirty));
  } else {
    dirty.set("status", Json::string("unavailable"));
    dirty.set("reason", Json::string(record.source_dirty_reason));
  }
  json.set("source_dirty", std::move(dirty));
  json.set("source_digest", Json::string(record.source_digest));
  json.set("source_digest_reason", Json::string(record.source_digest_reason));
  json.set("library_path", Json::string(record.library_path));
  json.set("library_build_type", Json::string(record.library_build_type));
  json.set("library_digest", Json::string(record.library_digest));
  json.set("library_reason", Json::string(record.library_reason));
  json.set("cache_hit", Json::boolean(record.cache_hit));
  json.set("cache_build_id", Json::string(record.cache_build_id));
  json.set("artifact_digest", Json::string(record.artifact_digest));
  json.set("started_at", Json::string(record.started_at));
  json.set("completed_at", Json::string(record.completed_at));
  json.set("exit_code", Json::integer(record.exit_code));
  json.set("success", Json::boolean(success));
  json.set("message", Json::string(message));
  return json;
}

void fill_source_digest(const Manifest& manifest, const std::string& dependency_path, BuildRecord* record) {
  if (!path_exists(dependency_path)) {
    record->source_digest = "unavailable";
    record->source_digest_reason = "compiler dependency file is absent";
    return;
  }
  const std::vector<std::string> prerequisites = parse_make_dependencies(read_file(dependency_path));
  if (prerequisites.empty()) {
    record->source_digest = "unavailable";
    record->source_digest_reason = "dependency file lists no prerequisites";
    return;
  }
  std::vector<std::string> paths = prerequisites;
  for (const std::string& source : manifest.sources) {
    paths.push_back(join_path(manifest.directory, source));
  }
  record->source_digest = digest_files(paths, &record->source_digest_reason);
}

std::string library_folder(const std::string& build_type) {
  if (build_type == "Release") {
    return "release";
  }
  if (build_type == "RelWithDebInfo") {
    return "relwithdebinfo";
  }
  return "debug";
}

std::string ensure_syslab_library(const BuildContext& context, const std::string& build_type) {
  if (build_type == "Debug") {
    const std::string beside_cli = join_path(context.library_dir, "libsyslab.a");
    if (path_exists(beside_cli)) {
      return beside_cli;
    }
  }
  const std::string build_dir = join_path(join_path(context.source_root, "build"), library_folder(build_type));
  const std::string archive = join_path(build_dir, "libsyslab.a");
  if (path_exists(archive)) {
    return archive;
  }
  long processors = ::sysconf(_SC_NPROCESSORS_ONLN);
  if (processors < 1) {
    processors = 1;
  }
  CommandRequest configure;
  configure.argv = {"cmake", "-S", context.source_root, "-B", build_dir, "-DCMAKE_BUILD_TYPE=" + build_type,
                    "-DCMAKE_CXX_COMPILER=" + context.compiler};
  configure.inherit_environment = true;
  configure.timeout_ms = 120000;
  configure.output_limit = 2 * 1024 * 1024;
  const CommandResult configured = run_command(configure);
  if (configured.exit_code != 0) {
    const std::string log = configured.stdout_text + configured.stderr_text + configured.spawn_error;
    throw Error(Error::Code::Build, "cmake configure failed for " + build_type + ": " + log);
  }
  CommandRequest build;
  build.argv = {"cmake", "--build", build_dir, "--target", "syslab", "-j", std::to_string(processors)};
  build.inherit_environment = true;
  build.timeout_ms = 600000;
  build.output_limit = 4 * 1024 * 1024;
  const CommandResult built = run_command(build);
  if (built.exit_code != 0 || !path_exists(archive)) {
    const std::string log = built.stdout_text + built.stderr_text + built.spawn_error;
    throw Error(Error::Code::Build, "cmake build failed for " + build_type + ": " + log);
  }
  return archive;
}

void assign_library(BuildRecord* record, bool link_syslab, const std::string& path, const std::string& build_type) {
  if (!link_syslab) {
    record->library_path = "unavailable";
    record->library_build_type = "unavailable";
    record->library_digest = "unavailable";
    record->library_reason = "manifest does not link libsyslab.a";
    return;
  }
  record->library_path = path;
  record->library_build_type = build_type;
  record->library_digest = to_hex(sha256_file(path));
  record->library_reason.clear();
}

std::vector<std::string> normalize_arguments(const std::vector<std::string>& arguments) {
  std::vector<std::string> normalized;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if ((arguments[index] == "-MF" || arguments[index] == "-o") && index + 1 < arguments.size()) {
      normalized.push_back(arguments[index]);
      normalized.push_back(arguments[index] == "-MF" ? "@deps" : "@output");
      ++index;
      continue;
    }
    normalized.push_back(arguments[index]);
  }
  return normalized;
}

std::string hash_parts(const std::vector<std::string>& parts) {
  std::string payload;
  for (const std::string& part : parts) {
    append_u64(&payload, part.size());
    payload.append(part);
  }
  return to_hex(sha256(payload));
}

std::string cache_prefix(const BuildRecord& record, const std::string& source_digest) {
  std::vector<std::string> parts = {record.compiler_path, record.compiler_version, record.library_digest};
  const std::vector<std::string> compile = normalize_arguments(record.compile_arguments);
  const std::vector<std::string> link = normalize_arguments(record.link_arguments);
  parts.insert(parts.end(), compile.begin(), compile.end());
  parts.insert(parts.end(), link.begin(), link.end());
  parts.push_back(source_digest);
  return hash_parts(parts);
}

std::string json_string_field(const Json& json, const char* key) {
  const Json* field = json.find(key);
  if (field == nullptr || field->type() != Json::Type::String) {
    return {};
  }
  return field->as_string();
}

bool load_cached_record(const Json& json, BuildRecord* record) {
  record->build_id = json_string_field(json, "build_id");
  record->experiment_id = json_string_field(json, "experiment_id");
  record->compiler_path = json_string_field(json, "compiler_path");
  record->compiler_version = json_string_field(json, "compiler_version");
  record->target_triple = json_string_field(json, "target_triple");
  record->language_standard = json_string_field(json, "language_standard");
  record->source_revision = json_string_field(json, "source_revision");
  record->source_revision_reason = json_string_field(json, "source_revision_reason");
  record->source_digest = json_string_field(json, "source_digest");
  record->source_digest_reason = json_string_field(json, "source_digest_reason");
  record->library_path = json_string_field(json, "library_path");
  record->library_build_type = json_string_field(json, "library_build_type");
  record->library_digest = json_string_field(json, "library_digest");
  record->library_reason = json_string_field(json, "library_reason");
  record->artifact_digest = json_string_field(json, "artifact_digest");
  record->started_at = json_string_field(json, "started_at");
  record->completed_at = json_string_field(json, "completed_at");
  const Json* dirty = json.find("source_dirty");
  if (dirty != nullptr && dirty->type() == Json::Type::Object) {
    const std::string status = json_string_field(*dirty, "status");
    record->source_dirty_known = status == "ok";
    record->source_dirty_reason = json_string_field(*dirty, "reason");
    const Json* value = dirty->find("value");
    record->source_dirty = value != nullptr && value->type() == Json::Type::Bool && value->as_bool();
  }
  const Json* compile = json.find("compile_arguments");
  const Json* link = json.find("link_arguments");
  if (compile == nullptr || link == nullptr || compile->type() != Json::Type::Array || link->type() != Json::Type::Array) {
    return false;
  }
  for (const Json& item : compile->as_array()) {
    if (item.type() != Json::Type::String) {
      return false;
    }
    record->compile_arguments.push_back(item.as_string());
  }
  for (const Json& item : link->as_array()) {
    if (item.type() != Json::Type::String) {
      return false;
    }
    record->link_arguments.push_back(item.as_string());
  }
  const Json* exit_code = json.find("exit_code");
  if (exit_code != nullptr && exit_code->type() == Json::Type::Integer) {
    record->exit_code = static_cast<int>(exit_code->as_integer());
  }
  return !record->build_id.empty() && !record->artifact_digest.empty();
}

std::vector<std::string> header_paths(const Manifest& manifest, const std::vector<std::string>& prerequisites) {
  std::vector<std::string> sources;
  for (const std::string& source : manifest.sources) {
    sources.push_back(join_path(manifest.directory, source));
  }
  std::vector<std::string> headers;
  for (const std::string& path : prerequisites) {
    bool is_source = false;
    for (const std::string& source : sources) {
      if (path == source) {
        is_source = true;
      }
    }
    if (!is_source) {
      headers.push_back(path);
    }
  }
  return headers;
}

bool reuse_cache(const Manifest& manifest, const BuildRecord& probe, const std::string& source_only_digest,
                 const std::string& cache_root, BuiltExperiment* built) {
  const std::string prefix = cache_prefix(probe, source_only_digest);
  const std::string prefix_dir = join_path(cache_root, prefix);
  if (!path_exists(prefix_dir)) {
    return false;
  }
  for (const std::string& name : list_directory(prefix_dir)) {
    const std::string entry_dir = join_path(prefix_dir, name);
    const std::string entry_path = join_path(entry_dir, "entry.json");
    if (!path_exists(entry_path)) {
      continue;
    }
    Json entry;
    try {
      entry = Json::parse(read_file(entry_path));
    } catch (const Error&) {
      remove_tree(entry_dir);
      continue;
    }
    const Json* listed = entry.find("prerequisites");
    if (listed == nullptr || listed->type() != Json::Type::Array) {
      continue;
    }
    std::vector<std::string> prerequisites;
    bool paths_ok = true;
    for (const Json& item : listed->as_array()) {
      if (item.type() != Json::Type::String) {
        paths_ok = false;
        break;
      }
      prerequisites.push_back(item.as_string());
    }
    if (!paths_ok) {
      continue;
    }
    std::string header_reason;
    const std::string headers = [&]() {
      const std::vector<std::string> only_headers = header_paths(manifest, prerequisites);
      if (only_headers.empty()) {
        return to_hex(sha256(std::string()));
      }
      return digest_files(only_headers, &header_reason);
    }();
    if (!header_reason.empty() || headers == "unavailable") {
      continue;
    }
    const std::string full_key = hash_parts({prefix, headers});
    if (name != full_key) {
      continue;
    }
    const std::string executable = json_string_field(entry, "executable");
    BuildRecord cached;
    if (!load_cached_record(entry, &cached) || executable.empty() || !path_exists(executable)) {
      remove_tree(entry_dir);
      return false;
    }
    const std::string actual = to_hex(sha256_file(executable));
    if (actual != cached.artifact_digest) {
      remove_tree(entry_dir);
      return false;
    }
    built->record = cached;
    built->record.cache_hit = true;
    built->record.cache_build_id = cached.build_id;
    built->record.build_id = make_id("build");
    built->directory = join_path(parent_path(cache_root), "builds/" + built->record.build_id);
    create_directories(built->directory);
    built->executable = executable;
    built->success = true;
    built->message = "使用构建缓存";
    built->record.exit_code = 0;
    write_file(join_path(built->directory, "build.json"),
               build_record_json(built->record, true, built->message).dump());
    return true;
  }
  return false;
}

void store_cache(const Manifest& manifest, const BuiltExperiment& built, const std::string& dependency_path,
                 const std::string& source_only_digest, const std::string& cache_root) {
  if (!path_exists(dependency_path)) {
    return;
  }
  const std::vector<std::string> prerequisites = parse_make_dependencies(read_file(dependency_path));
  std::string header_reason;
  const std::vector<std::string> headers = header_paths(manifest, prerequisites);
  const std::string header_digest =
      headers.empty() ? to_hex(sha256(std::string())) : digest_files(headers, &header_reason);
  if (!header_reason.empty() || header_digest == "unavailable") {
    return;
  }
  const std::string prefix = cache_prefix(built.record, source_only_digest);
  const std::string full_key = hash_parts({prefix, header_digest});
  const std::string entry_dir = join_path(join_path(cache_root, prefix), full_key);
  create_directories(entry_dir);
  Json entry = build_record_json(built.record, true, built.message);
  entry.set("executable", Json::string(built.executable));
  Json listed = Json::array();
  for (const std::string& path : prerequisites) {
    listed.push(Json::string(path));
  }
  entry.set("prerequisites", std::move(listed));
  write_file(join_path(entry_dir, "entry.json"), entry.dump());
}

struct CacheEntryAge {
  std::string directory;
  std::string build_id;
  std::string executable;
  bool executable_missing = false;
};

}  // namespace

int trim_build_cache(const std::string& artifacts_dir, std::size_t limit,
                     const std::vector<std::string>& keep_executables) {
  const std::string cache_root = join_path(artifacts_dir, "cache");
  if (!path_exists(cache_root)) {
    return 0;
  }
  std::vector<CacheEntryAge> entries;
  for (const std::string& prefix : list_directory(cache_root)) {
    const std::string prefix_dir = join_path(cache_root, prefix);
    if (!is_directory(prefix_dir)) {
      continue;
    }
    for (const std::string& key : list_directory(prefix_dir)) {
      const std::string entry_dir = join_path(prefix_dir, key);
      CacheEntryAge entry;
      entry.directory = entry_dir;
      const std::string entry_path = join_path(entry_dir, "entry.json");
      if (!path_exists(entry_path)) {
        entry.executable_missing = true;
      } else {
        try {
          const Json json = Json::parse(read_file(entry_path));
          entry.build_id = json_string_field(json, "build_id");
          entry.executable = json_string_field(json, "executable");
          entry.executable_missing = entry.executable.empty() || !path_exists(entry.executable);
        } catch (const Error&) {
          entry.executable_missing = true;
        }
      }
      entries.push_back(std::move(entry));
    }
  }
  std::sort(entries.begin(), entries.end(), [](const CacheEntryAge& left, const CacheEntryAge& right) {
    if (left.executable_missing != right.executable_missing) {
      return left.executable_missing;
    }
    return left.build_id < right.build_id;
  });
  int removed = 0;
  std::size_t kept = 0;
  for (const CacheEntryAge& entry : entries) {
    const bool referenced = !entry.executable_missing &&
                            std::find(keep_executables.begin(), keep_executables.end(), entry.executable) !=
                                keep_executables.end();
    if (referenced) {
      continue;
    }
    const bool drop = entry.executable_missing || kept >= limit;
    if (drop) {
      remove_tree(entry.directory);
      ++removed;
    } else {
      ++kept;
    }
  }
  for (const std::string& prefix : list_directory(cache_root)) {
    const std::string prefix_dir = join_path(cache_root, prefix);
    if (is_directory(prefix_dir) && list_directory(prefix_dir).empty()) {
      remove_tree(prefix_dir);
    }
  }
  return removed;
}

std::vector<std::string> parse_make_dependencies(const std::string& text) {
  std::string joined;
  joined.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\\' && index + 1 < text.size() && (text[index + 1] == '\n' || text[index + 1] == '\r')) {
      if (text[index + 1] == '\r' && index + 2 < text.size() && text[index + 2] == '\n') {
        index += 2;
      } else {
        ++index;
      }
      continue;
    }
    joined.push_back(text[index]);
  }

  std::vector<std::string> tokens;
  std::string current;
  bool escaping = false;
  const auto flush = [&]() {
    if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  };
  for (char character : joined) {
    if (escaping) {
      current.push_back(character);
      escaping = false;
      continue;
    }
    if (character == '\\') {
      escaping = true;
      continue;
    }
    if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
      flush();
      continue;
    }
    current.push_back(character);
  }
  if (escaping) {
    current.push_back('\\');
  }
  flush();

  std::vector<std::string> prerequisites;
  bool after_colon = false;
  for (const std::string& token : tokens) {
    if (token == ":") {
      after_colon = true;
      continue;
    }
    if (!token.empty() && token.back() == ':') {
      after_colon = true;
      continue;
    }
    if (after_colon) {
      prerequisites.push_back(token);
    }
  }
  return prerequisites;
}

std::string digest_files(const std::vector<std::string>& paths, std::string* reason) {
  if (reason == nullptr) {
    throw Error(Error::Code::InvalidArgument, "digest reason pointer is null");
  }
  reason->clear();
  std::vector<std::string> ordered = paths;
  std::sort(ordered.begin(), ordered.end());
  ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
  if (ordered.empty()) {
    *reason = "no files were provided for the source digest";
    return "unavailable";
  }
  std::string payload;
  for (const std::string& path : ordered) {
    if (!path_exists(path)) {
      *reason = "source file is missing: " + path;
      return "unavailable";
    }
    std::string bytes;
    try {
      bytes = read_file(path);
    } catch (const Error& error) {
      *reason = error.what();
      return "unavailable";
    }
    append_u64(&payload, path.size());
    payload.append(path);
    append_u64(&payload, bytes.size());
    payload.append(bytes);
  }
  return to_hex(sha256(payload));
}

BuiltExperiment build_experiment(const Manifest& manifest, const VariantSpec& variant,
                                 const BuildContext& context) {
  if (!path_exists(context.compiler)) {
    throw Error(Error::Code::NotFound, "compiler does not exist: " + context.compiler);
  }
  BuiltExperiment built;
  built.record.build_id = make_id("build");
  built.record.experiment_id = manifest.experiment.id;
  built.record.compiler_path = context.compiler;
  built.record.compiler_version = probe_text(context.compiler, "--version");
  built.record.target_triple = probe_text(context.compiler, "-dumpmachine");
  built.record.language_standard = manifest.standard;
  built.record.started_at = utc_timestamp();
  built.record.source_revision = source_revision(context.source_root, &built.record.source_revision_reason);
  read_source_dirty(context.source_root, &built.record);
  built.directory = join_path(context.output_directory, built.record.build_id);
  create_directories(built.directory);
  built.executable = join_path(built.directory, manifest.executable);
  const std::string dependency_path = join_path(built.directory, "dependencies.d");

  std::vector<std::string> compile_arguments = {context.compiler, "-std=" + manifest.standard, "-pthread", "-Wall",
                                                "-Wextra", "-Wno-deprecated-declarations"};
  compile_arguments.push_back("-I");
  compile_arguments.push_back(join_path(context.source_root, "include"));
  compile_arguments.push_back("-I");
  compile_arguments.push_back(context.source_root);
  if (!context.polyfill_dir.empty()) {
    compile_arguments.push_back("-I");
    compile_arguments.push_back(context.polyfill_dir);
  }
  const std::vector<std::string> defaults = optimization_flags(variant.build_type);
  compile_arguments.insert(compile_arguments.end(), defaults.begin(), defaults.end());
  for (const std::string& flag : variant.flags) {
    if (flag.empty() || flag.find('\n') != std::string::npos) {
      throw Error(Error::Code::Validation, "variant flag is empty or contains a newline");
    }
    compile_arguments.push_back(flag);
  }
  compile_arguments.push_back("-MD");
  compile_arguments.push_back("-MF");
  compile_arguments.push_back(dependency_path);
  for (const std::string& source : manifest.sources) {
    compile_arguments.push_back(join_path(manifest.directory, source));
  }
  std::vector<std::string> link_arguments = {"-pthread", "-o", built.executable};
  bool link_syslab = false;
  for (const std::string& library : manifest.link_libraries) {
    if (library == "syslab") {
      link_syslab = true;
    } else {
      link_arguments.push_back("-l" + library);
    }
  }
  if (link_syslab) {
    link_arguments.push_back(ensure_syslab_library(context, variant.build_type));
  }
  assign_library(&built.record, link_syslab, link_syslab ? link_arguments.back() : std::string(), variant.build_type);
  std::vector<std::string> argv = compile_arguments;
  argv.insert(argv.end(), link_arguments.begin(), link_arguments.end());
  built.record.compile_arguments = compile_arguments;
  built.record.link_arguments = link_arguments;

  std::vector<std::string> manifest_sources;
  for (const std::string& source : manifest.sources) {
    manifest_sources.push_back(join_path(manifest.directory, source));
  }
  std::string source_only_reason;
  const std::string source_only_digest = digest_files(manifest_sources, &source_only_reason);
  const std::string cache_root = join_path(parent_path(context.output_directory), "cache");
  if (context.use_cache && source_only_reason.empty()) {
    BuiltExperiment cached;
    if (reuse_cache(manifest, built.record, source_only_digest, cache_root, &cached)) {
      remove_tree(built.directory);
      return cached;
    }
  }

  CommandRequest request;
  request.argv = argv;
  request.inherit_environment = true;
  request.timeout_ms = 120000;
  request.output_limit = 2 * 1024 * 1024;
  request.work_directory = manifest.directory;
  const CommandResult result = run_command(request);
  const std::string log = result.stdout_text + result.stderr_text;
  write_file(join_path(built.directory, "build.log"), log);
  built.record.completed_at = utc_timestamp();
  built.record.exit_code = result.exit_code;
  if (result.exit_code == 0 && path_exists(built.executable)) {
    built.record.artifact_digest = to_hex(sha256_file(built.executable));
    built.success = true;
    built.message = "构建完成";
  } else {
    built.success = false;
    built.message = result.spawn_error.empty() ? "编译失败" : result.spawn_error;
    if (built.record.artifact_digest.empty()) {
      built.record.artifact_digest = "unavailable";
    }
  }
  fill_source_digest(manifest, dependency_path, &built.record);
  if (built.record.source_digest.empty()) {
    built.record.source_digest = "unavailable";
  }
  if (built.success && context.use_cache && source_only_reason.empty()) {
    store_cache(manifest, built, dependency_path, source_only_digest, cache_root);
  }
  write_file(join_path(built.directory, "build.json"),
             build_record_json(built.record, built.success, built.message).dump());
  return built;
}

}  // namespace syslab
