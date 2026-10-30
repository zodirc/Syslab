#include "syslab/build/build_manager.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/time.hpp"
#include "syslab/runner/process.hpp"

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

}  // namespace

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
  built.directory = join_path(context.output_directory, built.record.build_id);
  create_directories(built.directory);
  built.executable = join_path(built.directory, manifest.executable);

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
    const std::string archive = join_path(context.library_dir, "libsyslab.a");
    if (!path_exists(archive)) {
      throw Error(Error::Code::NotFound, "libsyslab.a was not found in " + context.library_dir);
    }
    link_arguments.push_back(archive);
  }
  std::vector<std::string> argv = compile_arguments;
  argv.insert(argv.end(), link_arguments.begin(), link_arguments.end());
  built.record.compile_arguments = compile_arguments;
  built.record.link_arguments = link_arguments;

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
  Json record = Json::object();
  record.set("build_id", Json::string(built.record.build_id));
  record.set("experiment_id", Json::string(built.record.experiment_id));
  record.set("compiler_path", Json::string(built.record.compiler_path));
  record.set("compiler_version", Json::string(built.record.compiler_version));
  record.set("target_triple", Json::string(built.record.target_triple));
  record.set("language_standard", Json::string(built.record.language_standard));
  record.set("source_revision", Json::string(built.record.source_revision));
  record.set("source_revision_reason", Json::string(built.record.source_revision_reason));
  record.set("artifact_digest", Json::string(built.record.artifact_digest));
  record.set("started_at", Json::string(built.record.started_at));
  record.set("completed_at", Json::string(built.record.completed_at));
  record.set("exit_code", Json::integer(built.record.exit_code));
  record.set("success", Json::boolean(built.success));
  record.set("message", Json::string(built.message));
  write_file(join_path(built.directory, "build.json"), record.dump());
  return built;
}

}  // namespace syslab
