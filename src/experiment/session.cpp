#include "syslab/experiment/session.hpp"

#include "syslab/build/build_manager.hpp"
#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/time.hpp"
#include "syslab/experiment/lifecycle.hpp"
#include "syslab/observation/elf.hpp"
#include "syslab/observation/protocol.hpp"
#include "syslab/observation/report.hpp"
#include "syslab/platform/capability.hpp"
#include "syslab/platform/environment.hpp"
#include "syslab/runner/process.hpp"

#include <sys/utsname.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace syslab {
namespace {

std::string host_architecture() {
  utsname system {};
  if (uname(&system) != 0) {
    return {};
  }
  std::string machine = system.machine;
  if (machine == "arm64") {
    machine = "aarch64";
  }
  return machine;
}

Json run_to_json(const RunRecord& run) {
  Json json = Json::object();
  json.set("run_id", Json::string(run.run_id));
  json.set("experiment_id", Json::string(run.experiment_id));
  json.set("build_id", Json::string(run.build_id));
  json.set("environment_id", Json::string(run.environment_id));
  json.set("started_at", Json::string(run.started_at));
  json.set("completed_at", Json::string(run.completed_at));
  json.set("duration_ns", Json::integer(static_cast<std::int64_t>(run.duration_ns)));
  json.set("exit_code", Json::integer(run.exit_code));
  json.set("term_signal", Json::integer(run.term_signal));
  json.set("timed_out", Json::boolean(run.timed_out));
  json.set("interrupted", Json::boolean(run.interrupted));
  json.set("output_limited", Json::boolean(run.output_limited));
  json.set("termination", Json::string(run.termination));
  json.set("stdout_artifact", Json::string(run.stdout_artifact));
  json.set("stderr_artifact", Json::string(run.stderr_artifact));
  return json;
}

Json observation_to_json(const ObservationRecord& observation) {
  Json json = Json::object();
  json.set("observation_id", Json::string(observation.observation_id));
  json.set("run_id", Json::string(observation.run_id));
  json.set("kind", Json::string(to_string(observation.kind)));
  json.set("tool_name", Json::string(observation.tool_name));
  json.set("tool_version", Json::string(observation.tool_version));
  json.set("tool_path", Json::string(observation.tool_path));
  json.set("artifact_path", Json::string(observation.artifact_path));
  json.set("content_digest", Json::string(observation.content_digest));
  json.set("collected_at", Json::string(observation.collected_at));
  json.set("complete", Json::boolean(observation.complete));
  json.set("limitation", Json::string(observation.limitation));
  return json;
}

Json check_to_json(const VerificationResult& check) {
  Json json = Json::object();
  json.set("check_id", Json::string(check.check_id));
  json.set("run_id", Json::string(check.run_id));
  json.set("status", Json::string(to_string(check.status)));
  json.set("expected", Json::string(check.expected));
  json.set("actual", Json::string(check.actual));
  json.set("explanation", Json::string(check.explanation));
  return json;
}

bool contains_text(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::string protocol_errors(const ProtocolRecord& protocol) {
  std::string text;
  for (const ProtocolDiagnostic& error : protocol.errors) {
    if (!text.empty()) {
      text += "; ";
    }
    text += "line " + std::to_string(error.line) + ": " + error.message;
  }
  return text;
}

VerificationResult evaluate_check(const CheckSpec& spec, const RunRecord& run, const std::string& stdout_text,
                                  const std::string& stderr_text) {
  VerificationResult result;
  result.check_id = spec.name;
  result.run_id = run.run_id;
  result.expected = spec.expected;
  if (spec.type == "exit_code") {
    result.actual = std::to_string(run.exit_code);
    result.status = result.actual == spec.expected ? VerificationStatus::Passed : VerificationStatus::Failed;
    result.explanation = "exit status compared with the manifest";
  } else if (spec.type == "stdout_contains") {
    result.actual = contains_text(stdout_text, spec.expected) ? "present" : "absent";
    result.status = result.actual == "present" ? VerificationStatus::Passed : VerificationStatus::Failed;
    result.explanation = "stdout substring check";
  } else if (spec.type == "stderr_contains") {
    result.actual = contains_text(stderr_text, spec.expected) ? "present" : "absent";
    result.status = result.actual == "present" ? VerificationStatus::Passed : VerificationStatus::Failed;
    result.explanation = "stderr substring check";
  } else if (spec.type == "result_marker") {
    const ProtocolRecord protocol = parse_protocol(stdout_text);
    const std::string format = protocol_errors(protocol);
    result.actual = protocol.status.empty() ? "missing" : protocol.status;
    if (!format.empty()) {
      result.status = VerificationStatus::Failed;
      result.explanation = format;
    } else if (protocol.status.empty()) {
      result.status = VerificationStatus::Failed;
      result.explanation = "SYSLAB_RESULT marker was not printed";
    } else if (protocol.status == "unsupported") {
      result.status = VerificationStatus::Unsupported;
      result.explanation = "experiment reported that the host cannot perform the observation";
    } else if (protocol.status == spec.expected) {
      result.status = VerificationStatus::Passed;
      result.explanation = "result marker matched";
    } else {
      result.status = VerificationStatus::Failed;
      result.explanation = "result marker did not match";
    }
  } else {
    result.status = VerificationStatus::Failed;
    result.actual = "unknown-check";
    result.explanation = "check type is not implemented";
  }
  return result;
}

ObservationRecord file_observation(const std::string& run_id, ObservationKind kind, const std::string& tool,
                                   const std::string& version, const std::string& path, bool complete,
                                   const std::string& limitation, const std::string& resolved_tool = {}) {
  ObservationRecord record;
  record.observation_id = make_id("obs");
  record.run_id = run_id;
  record.kind = kind;
  record.tool_name = tool;
  record.tool_version = version;
  record.tool_path = resolved_tool;
  record.artifact_path = path;
  record.collected_at = utc_timestamp();
  record.complete = complete;
  record.limitation = limitation;
  if (complete && path_exists(path)) {
    record.content_digest = to_hex(sha256_file(path));
  } else {
    record.content_digest = "unavailable";
  }
  return record;
}

std::string refusal_reason(const Manifest& manifest) {
  if (manifest.experiment.risk == RiskLevel::KernelPrivileged ||
      manifest.experiment.risk == RiskLevel::BareMetal) {
    return "日常主机拒绝内核特权实验和裸机实验";
  }
  const std::string architecture = host_architecture();
  bool supported = false;
  for (const std::string& item : manifest.architectures) {
    if (item == architecture) {
      supported = true;
    }
  }
  if (!supported) {
    return "主机架构 " + architecture + " 不在清单允许的范围内";
  }
  const std::vector<Capability> capabilities = probe_capabilities();
  for (const std::string& required : manifest.experiment.required_capabilities) {
    const Capability* capability = find_capability(capabilities, required);
    if (capability == nullptr || capability->status != "ok") {
      return "所需能力不可用: " + required;
    }
  }
  return {};
}

void publish_latest(const std::string& artifacts, const std::string& id, const std::string& directory,
                    const std::string& build_id) {
  const std::string latest = join_path(artifacts, "latest");
  create_directories(latest);
  write_file(join_path(latest, id + ".txt"), directory + "\n");
  Json line = Json::object();
  line.set("experiment_id", Json::string(id));
  line.set("directory", Json::string(directory));
  line.set("build_id", Json::string(build_id));
  const std::string index_path = join_path(artifacts, "index.jsonl");
  append_file(index_path, line.dump(0) + "\n");
}

struct RunDirectory {
  std::string name;
  std::string path;
  std::string experiment_id;
  std::string build_id;
};

std::string json_string(const Json& json, const std::string& key) {
  const Json* field = json.find(key);
  if (field == nullptr || field->type() != Json::Type::String) {
    return {};
  }
  return field->as_string();
}

void read_run_identity(RunDirectory* run) {
  for (const std::string& name : list_directory(run->path)) {
    if (name.size() < 5 || name.compare(name.size() - 5, 5, ".json") != 0) {
      continue;
    }
    if (name == "observations.json" || name == "verification.json" || name == "environment.json" ||
        name == "lifecycle.json" || name == "elf.json") {
      continue;
    }
    try {
      const Json json = Json::parse(read_file(join_path(run->path, name)));
      const std::string experiment_id = json_string(json, "experiment_id");
      const std::string build_id = json_string(json, "build_id");
      if (!experiment_id.empty() && !build_id.empty()) {
        run->experiment_id = experiment_id;
        run->build_id = build_id;
        return;
      }
    } catch (const Error&) {
    }
  }
}

std::string linked_executable(const std::string& build_json_path) {
  if (!path_exists(build_json_path)) {
    return {};
  }
  try {
    const Json json = Json::parse(read_file(build_json_path));
    const Json* arguments = json.find("link_arguments");
    if (arguments == nullptr || arguments->type() != Json::Type::Array) {
      return {};
    }
    const std::vector<Json>& values = arguments->as_array();
    for (std::size_t index = 0; index + 1 < values.size(); ++index) {
      if (values[index].type() == Json::Type::String && values[index].as_string() == "-o" &&
          values[index + 1].type() == Json::Type::String) {
        return values[index + 1].as_string();
      }
    }
  } catch (const Error&) {
  }
  return {};
}

}  // namespace

CleanReport clean_artifacts(const std::string& artifacts_dir, std::size_t keep) {
  CleanReport report;
  if (!path_exists(artifacts_dir)) {
    return report;
  }
  const std::string runs_root = join_path(artifacts_dir, "runs");
  std::map<std::string, std::vector<RunDirectory>> groups;
  if (path_exists(runs_root)) {
    for (const std::string& name : list_directory(runs_root)) {
      const std::string path = join_path(runs_root, name);
      if (!is_directory(path) || name.compare(0, 6, "batch-") != 0) {
        continue;
      }
      RunDirectory run;
      run.name = name;
      run.path = path;
      read_run_identity(&run);
      groups[run.experiment_id].push_back(std::move(run));
    }
  }

  std::vector<RunDirectory> kept;
  std::set<std::string> keep_builds;
  for (auto& group : groups) {
    std::vector<RunDirectory>& entries = group.second;
    std::sort(entries.begin(), entries.end(),
              [](const RunDirectory& left, const RunDirectory& right) { return left.name < right.name; });
    const std::size_t drop_count = entries.size() > keep ? entries.size() - keep : 0;
    for (std::size_t index = 0; index < entries.size(); ++index) {
      if (index < drop_count) {
        remove_tree(entries[index].path);
        ++report.runs_removed;
      } else {
        if (!entries[index].build_id.empty()) {
          keep_builds.insert(entries[index].build_id);
        }
        kept.push_back(entries[index]);
      }
    }
  }

  const std::string builds_root = join_path(artifacts_dir, "builds");
  std::vector<std::string> protected_executables;
  std::set<std::string> extra_builds;
  for (const std::string& build_id : keep_builds) {
    const std::string record_path = join_path(join_path(builds_root, build_id), "build.json");
    if (!path_exists(record_path)) {
      continue;
    }
    try {
      const Json json = Json::parse(read_file(record_path));
      const std::string cached = json_string(json, "cache_build_id");
      if (!cached.empty()) {
        extra_builds.insert(cached);
      }
    } catch (const Error&) {
    }
    const std::string executable = linked_executable(record_path);
    if (!executable.empty()) {
      protected_executables.push_back(executable);
    }
  }
  keep_builds.insert(extra_builds.begin(), extra_builds.end());
  for (const std::string& build_id : extra_builds) {
    const std::string executable = linked_executable(join_path(join_path(builds_root, build_id), "build.json"));
    if (!executable.empty()) {
      protected_executables.push_back(executable);
    }
  }

  if (path_exists(builds_root)) {
    for (const std::string& name : list_directory(builds_root)) {
      const std::string path = join_path(builds_root, name);
      if (!is_directory(path) || keep_builds.count(name) != 0) {
        continue;
      }
      remove_tree(path);
      ++report.builds_removed;
    }
  }

  std::sort(kept.begin(), kept.end(),
            [](const RunDirectory& left, const RunDirectory& right) { return left.name < right.name; });
  std::string index_body;
  std::set<std::string> kept_experiments;
  const std::string latest_root = join_path(artifacts_dir, "latest");
  create_directories(latest_root);
  for (const RunDirectory& run : kept) {
    Json line = Json::object();
    line.set("experiment_id", Json::string(run.experiment_id));
    line.set("directory", Json::string(run.path));
    line.set("build_id", Json::string(run.build_id));
    index_body += line.dump(0);
    index_body += '\n';
    if (!run.experiment_id.empty()) {
      kept_experiments.insert(run.experiment_id);
    }
  }
  std::map<std::string, std::string> newest_run;
  for (const RunDirectory& run : kept) {
    if (!run.experiment_id.empty()) {
      newest_run[run.experiment_id] = run.path;
    }
  }
  for (const auto& item : newest_run) {
    write_file(join_path(latest_root, item.first + ".txt"), item.second + "\n");
  }
  if (path_exists(latest_root)) {
    for (const std::string& name : list_directory(latest_root)) {
      if (name.size() < 4 || name.compare(name.size() - 4, 4, ".txt") != 0) {
        continue;
      }
      const std::string id = name.substr(0, name.size() - 4);
      if (kept_experiments.count(id) == 0) {
        remove_tree(join_path(latest_root, name));
      }
    }
  }
  const std::string index_path = join_path(artifacts_dir, "index.jsonl");
  const std::string temporary = index_path + ".tmp";
  write_file(temporary, index_body);
  if (std::rename(temporary.c_str(), index_path.c_str()) != 0) {
    throw Error(Error::Code::Io, "无法替换运行索引");
  }
  report.cache_removed = trim_build_cache(artifacts_dir, kBuildCacheLimit, protected_executables);
  return report;
}

SessionOutput validate_session(const Manifest& manifest, const SessionConfig&) {
  SessionOutput output;
  const std::string reason = refusal_reason(manifest);
  if (!reason.empty()) {
    output.state = ExperimentState::Unsupported;
    output.failure_class = "unsupported";
    output.message = reason;
  } else {
    output.state = ExperimentState::Passed;
    output.message = "清单可以在本机尝试";
  }
  output.summary = manifest_to_json(manifest);
  output.summary.set("state", Json::string(to_string(output.state)));
  output.summary.set("message", Json::string(output.message));
  return output;
}

SessionOutput build_session(const Manifest& manifest, const SessionConfig& config) {
  SessionOutput output = validate_session(manifest, config);
  if (output.state == ExperimentState::Unsupported) {
    return output;
  }
  BuildContext context;
  context.source_root = config.source_root;
  context.compiler = config.compiler;
  context.library_dir = config.library_dir;
  context.polyfill_dir = config.polyfill_dir;
  context.output_directory = join_path(config.artifacts_dir, "builds");
  context.use_cache = config.use_cache;
  const BuiltExperiment built = build_experiment(manifest, find_variant(manifest, config.variant), context);
  output.run_directory = built.directory;
  output.message = built.message;
  output.state = built.success ? ExperimentState::Built : ExperimentState::Failed;
  output.failure_class = built.success ? "" : "build";
  output.summary = manifest_to_json(manifest);
  output.summary.set("state", Json::string(to_string(output.state)));
  output.summary.set("build_id", Json::string(built.record.build_id));
  output.summary.set("message", Json::string(output.message));
  return output;
}

SessionOutput finish_session(Lifecycle& lifecycle, SessionOutput output, ExperimentState state,
                             const std::string& failure_class, const std::string& message,
                             const std::string& reason) {
  lifecycle.transition(state, reason);
  output.state = state;
  output.failure_class = failure_class;
  output.message = message;
  output.summary.set("state", Json::string(to_string(state)));
  output.summary.set("failure_class", Json::string(failure_class));
  output.summary.set("message", Json::string(message));
  output.summary.set("lifecycle", lifecycle.history());
  return output;
}

SessionOutput execute_session(const Manifest& manifest, const SessionConfig& config) {
  Lifecycle lifecycle(manifest.experiment.id);
  SessionOutput output;
  output.summary = manifest_to_json(manifest);
  lifecycle.transition(ExperimentState::Validating, "manifest loaded by the registry");
  const std::string refusal = refusal_reason(manifest);
  lifecycle.transition(ExperimentState::EnvironmentCheck, refusal.empty() ? "host matches the manifest" : refusal);
  if (!refusal.empty()) {
    return finish_session(lifecycle, std::move(output), ExperimentState::Unsupported, "unsupported", refusal,
                          refusal);
  }
  lifecycle.transition(ExperimentState::Building, "compiling the selected variant");
  BuildContext context;
  context.source_root = config.source_root;
  context.compiler = config.compiler;
  context.library_dir = config.library_dir;
  context.polyfill_dir = config.polyfill_dir;
  context.output_directory = join_path(config.artifacts_dir, "builds");
  context.use_cache = config.use_cache;
  const VariantSpec& variant = find_variant(manifest, config.variant);
  BuiltExperiment built = build_experiment(manifest, variant, context);
  if (!built.success) {
    output.run_directory = built.directory;
    return finish_session(lifecycle, std::move(output), ExperimentState::Failed, "build", built.message,
                          built.message);
  }
  lifecycle.transition(ExperimentState::Built, built.record.build_id);
  if (interrupt_requested()) {
    return finish_session(lifecycle, std::move(output), ExperimentState::Interrupted, "interrupted", "interrupted",
                          "interrupt requested before running");
  }
  lifecycle.transition(ExperimentState::Running, "starting experiment process");
  const int repetitions = config.repeat > 0 ? config.repeat : manifest.repetitions;
  const std::string run_directory = join_path(join_path(config.artifacts_dir, "runs"), make_id("batch"));
  create_directories(run_directory);
  EnvironmentSnapshot environment = capture_environment(config.compiler);
  write_file(join_path(run_directory, "environment.json"), environment_to_json(environment).dump());
  std::vector<RunRecord> runs;
  std::vector<std::string> stdouts;
  std::vector<std::string> stderrs;
  const bool want_strace = [&]() {
    for (const std::string& item : manifest.observe) {
      if (parse_observation_kind(item) == ObservationKind::SyscallTrace) {
        return true;
      }
    }
    return false;
  }();
  const bool want_perf = [&]() {
    for (const std::string& item : manifest.observe) {
      if (parse_observation_kind(item) == ObservationKind::PerformanceCounter) {
        return true;
      }
    }
    return false;
  }();
  const ResolvedTool strace_tool = want_strace ? resolve_tool("strace", {"-V"}) : ResolvedTool {};
  const ResolvedTool perf_tool = want_perf ? resolve_tool("perf", {"--version"}) : ResolvedTool {};
  const Capability perf_capability = want_perf ? probe_perf_events(perf_tool) : Capability {};
  const bool perf_ready = want_perf && perf_capability.status == "ok" && !manifest.perf_events.empty();
  std::string stdout_capture_error;
  std::string stderr_capture_error;
  for (int repetition = 0; repetition < repetitions; ++repetition) {
    if (interrupt_requested()) {
      break;
    }
    RunRecord run;
    run.run_id = make_id("run");
    run.experiment_id = manifest.experiment.id;
    run.build_id = built.record.build_id;
    run.environment_id = environment.environment_id;
    run.started_at = utc_timestamp();
    const std::string stdout_path = join_path(run_directory, run.run_id + ".stdout");
    const std::string stderr_path = join_path(run_directory, run.run_id + ".stderr");
    CommandRequest request;
    request.argv = {built.executable};
    request.argv.insert(request.argv.end(), manifest.arguments.begin(), manifest.arguments.end());
    if (perf_ready) {
      std::string events;
      for (const std::string& event : manifest.perf_events) {
        if (!events.empty()) {
          events += ",";
        }
        events += event;
      }
      const std::vector<std::string> inner = request.argv;
      request.argv = {perf_tool.path, "stat", "-x,", "-e", events, "-o",
                      join_path(run_directory, run.run_id + ".perf.csv"), "--"};
      request.argv.insert(request.argv.end(), inner.begin(), inner.end());
    }
    if (want_strace && strace_tool.available) {
      const std::vector<std::string> inner = request.argv;
      request.argv = {strace_tool.path, "-f", "-s", "80", "-o", join_path(run_directory, run.run_id + ".strace"),
                      "--"};
      request.argv.insert(request.argv.end(), inner.begin(), inner.end());
    }
    request.work_directory = run_directory;
    request.environment = sandbox_environment({{"SYSLAB_EXPERIMENT", manifest.experiment.id}});
    request.timeout_ms = manifest.limits.timeout_ms;
    request.output_limit = manifest.limits.output_bytes;
    request.memory_bytes = manifest.limits.memory_bytes;
    request.cpu_seconds = manifest.limits.cpu_seconds == 0
                              ? std::max<std::uint64_t>(1, manifest.limits.timeout_ms / 1000 + 1)
                              : manifest.limits.cpu_seconds;
    request.stdout_path = stdout_path;
    request.stderr_path = stderr_path;
    const CommandResult result = run_command(request);
    run.completed_at = utc_timestamp();
    run.duration_ns = result.duration_ns;
    run.exit_code = result.exit_code;
    run.term_signal = result.term_signal;
    run.timed_out = result.timed_out;
    run.interrupted = result.interrupted;
    run.output_limited = result.output_limited;
    run.termination = result.termination;
    run.stdout_artifact = stdout_path;
    run.stderr_artifact = stderr_path;
    stdout_capture_error = result.stdout_capture_error;
    stderr_capture_error = result.stderr_capture_error;
    if (!result.spawn_error.empty()) {
      write_file(stderr_path, result.spawn_error + "\n");
    }
    write_file(join_path(run_directory, run.run_id + ".json"), run_to_json(run).dump());
    stdouts.push_back(path_exists(stdout_path) ? read_file(stdout_path) : result.stdout_text);
    stderrs.push_back(path_exists(stderr_path) ? read_file(stderr_path) : result.stderr_text);
    runs.push_back(std::move(run));
  }
  if (runs.empty() && interrupt_requested()) {
    output.run_directory = run_directory;
    return finish_session(lifecycle, std::move(output), ExperimentState::Interrupted, "interrupted", "interrupted",
                          "interrupt requested before a run completed");
  }
  lifecycle.transition(ExperimentState::Observing, "collecting stdout, stderr, and optional tools");
  std::vector<ObservationRecord> observations;
  if (!runs.empty()) {
    const bool stdout_complete = stdout_capture_error.empty() && path_exists(runs.back().stdout_artifact);
    const bool stderr_complete = stderr_capture_error.empty() && path_exists(runs.back().stderr_artifact);
    const std::string stdout_limitation =
        !stdout_capture_error.empty() ? stdout_capture_error
                                      : (stdout_complete ? "" : "stdout artifact was not written");
    const std::string stderr_limitation =
        !stderr_capture_error.empty() ? stderr_capture_error
                                      : (stderr_complete ? "" : "stderr artifact was not written");
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::StandardOutput, "runner", "syslab",
                                            runs.back().stdout_artifact, stdout_complete, stdout_limitation));
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::StandardError, "runner", "syslab",
                                            runs.back().stderr_artifact, stderr_complete, stderr_limitation));
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::ExitStatus, "runner", "syslab",
                                            join_path(run_directory, runs.back().run_id + ".json"), true, ""));
    for (const std::string& item : manifest.observe) {
      const ObservationKind kind = parse_observation_kind(item);
      if (kind == ObservationKind::Disassembly) {
        const ResolvedTool objdump = resolve_tool("objdump", {"--version"});
        const std::string path = join_path(run_directory, "disassembly.txt");
        if (!objdump.available) {
          observations.push_back(file_observation(runs.back().run_id, kind, "objdump", objdump.version, path, false,
                                                  objdump.reason, objdump.path));
        } else {
          CommandRequest request;
          request.argv = {objdump.path, "-d", "--no-show-raw-insn", built.executable};
          request.inherit_environment = true;
          request.timeout_ms = 20000;
          request.stdout_path = path;
          request.output_limit = 8 * 1024 * 1024;
          const CommandResult result = run_command(request);
          const bool complete = result.exit_code == 0 && result.stdout_capture_error.empty();
          const std::string limitation = !result.stdout_capture_error.empty()
                                             ? result.stdout_capture_error
                                             : (complete ? "" : "objdump failed");
          observations.push_back(file_observation(runs.back().run_id, kind, "objdump", objdump.version, path, complete,
                                                  limitation, objdump.path));
        }
      } else if (kind == ObservationKind::ElfMetadata) {
        const ElfImage image = read_elf(built.executable);
        const std::string path = join_path(run_directory, "elf.json");
        write_file(path, elf_to_json(image).dump());
        observations.push_back(file_observation(runs.back().run_id, kind, "syslab-elf", "1", path, image.supported,
                                                image.limitation));
      } else if (kind == ObservationKind::SyscallTrace) {
        const std::string path = join_path(run_directory, runs.back().run_id + ".strace");
        if (!strace_tool.available) {
          observations.push_back(file_observation(runs.back().run_id, kind, "strace", strace_tool.version, path, false,
                                                  strace_tool.reason, strace_tool.path));
        } else {
          observations.push_back(file_observation(runs.back().run_id, kind, "strace", strace_tool.version, path,
                                                  path_exists(path),
                                                  path_exists(path) ? "" : "strace did not produce a trace",
                                                  strace_tool.path));
        }
      } else if (kind == ObservationKind::CompilerMetadata) {
        const std::string path = join_path(built.directory, "build.json");
        observations.push_back(file_observation(runs.back().run_id, kind, "compiler", built.record.compiler_version,
                                                path, path_exists(path),
                                                path_exists(path) ? "" : "build.json was not written"));
      } else if (kind == ObservationKind::PerformanceCounter) {
        const std::string path = join_path(run_directory, runs.back().run_id + ".perf.csv");
        bool complete = false;
        std::string limitation;
        if (!perf_tool.available || perf_capability.status != "ok") {
          limitation = !perf_capability.reason.empty() ? perf_capability.reason : perf_tool.reason;
          if (limitation.empty()) {
            limitation = "perf is unavailable";
          }
        } else if (!path_exists(path) || parse_perf_csv(read_file(path)).empty()) {
          limitation = stderrs.empty() || stderrs.back().empty() ? "perf stat did not write samples" : stderrs.back();
          const std::size_t newline = limitation.find('\n');
          if (newline != std::string::npos) {
            limitation = limitation.substr(0, newline);
          }
        } else {
          complete = true;
        }
        observations.push_back(file_observation(runs.back().run_id, kind, "perf",
                                                perf_tool.version.empty() ? "unavailable" : perf_tool.version, path,
                                                complete, limitation, perf_tool.path));
      } else if (kind == ObservationKind::SanitizerReport) {
        const bool sanitized = [&]() {
          for (const std::string& argument : built.record.compile_arguments) {
            if (argument.find("-fsanitize") != std::string::npos) {
              return true;
            }
          }
          return false;
        }();
        const std::string path = runs.back().stderr_artifact;
        if (sanitized) {
          observations.push_back(file_observation(runs.back().run_id, kind, "sanitizer", "stderr", path,
                                                  path_exists(path),
                                                  path_exists(path) ? "" : "sanitizer stderr was not captured"));
        } else {
          observations.push_back(file_observation(runs.back().run_id, kind, "sanitizer", "unavailable", path, false,
                                                  "this build was not compiled with a sanitizer"));
        }
      } else if (kind == ObservationKind::StandardOutput || kind == ObservationKind::StandardError ||
                 kind == ObservationKind::ExitStatus) {
      } else {
        observations.push_back(file_observation(
            runs.back().run_id, kind, "syslab", "unavailable", join_path(run_directory, "unavailable.txt"), false,
            std::string(to_string(kind)) + " was not sampled during this run"));
      }
    }
  }
  if (!runs.empty()) {
    bool sanitizer_build = false;
    for (const std::string& argument : built.record.compile_arguments) {
      if (argument.find("-fsanitize") != std::string::npos) {
        sanitizer_build = true;
      }
    }
    bool already_recorded = false;
    for (const ObservationRecord& observation : observations) {
      if (observation.kind == ObservationKind::SanitizerReport) {
        already_recorded = true;
      }
    }
    if (sanitizer_build && !already_recorded) {
      const std::string path = runs.back().stderr_artifact;
      observations.push_back(file_observation(runs.back().run_id, ObservationKind::SanitizerReport, "sanitizer",
                                              "stderr", path, path_exists(path),
                                              path_exists(path) ? "" : "sanitizer stderr was not captured"));
    }
  }
  Json observation_json = Json::array();
  for (const ObservationRecord& observation : observations) {
    observation_json.push(observation_to_json(observation));
  }
  write_file(join_path(run_directory, "observations.json"), observation_json.dump());
  lifecycle.transition(ExperimentState::Verifying, "evaluating manifest checks");
  std::vector<VerificationResult> checks;
  bool any_failed = false;
  bool any_unsupported = false;
  std::string failure_class;
  for (const RunRecord& run : runs) {
    if (run.timed_out) {
      failure_class = "timed_out";
    } else if (run.output_limited && failure_class.empty()) {
      failure_class = "output_limit";
    } else if (run.termination == "resource_limit" && failure_class.empty()) {
      failure_class = "resource_limit";
    }
  }
  for (std::size_t index = 0; index < runs.size(); ++index) {
    for (const CheckSpec& spec : manifest.checks) {
      VerificationResult check = evaluate_check(spec, runs[index], stdouts[index], stderrs[index]);
      if (check.status == VerificationStatus::Failed) {
        any_failed = true;
      }
      if (check.status == VerificationStatus::Unsupported) {
        any_unsupported = true;
      }
      checks.push_back(std::move(check));
    }
  }
  Json check_json = Json::array();
  for (const VerificationResult& check : checks) {
    check_json.push(check_to_json(check));
  }
  write_file(join_path(run_directory, "verification.json"), check_json.dump());
  if (interrupt_requested()) {
    lifecycle.transition(ExperimentState::Interrupted, "interrupt requested during verification");
    output.state = ExperimentState::Interrupted;
    failure_class = "interrupted";
  } else if (any_failed) {
    lifecycle.transition(ExperimentState::Failed, failure_class.empty() ? "verification failed" : failure_class);
    output.state = ExperimentState::Failed;
    if (failure_class.empty()) {
      failure_class = "verification";
    }
  } else if (any_unsupported) {
    lifecycle.transition(ExperimentState::Unsupported, "experiment reported unsupported");
    output.state = ExperimentState::Unsupported;
    failure_class = "unsupported";
  } else {
    lifecycle.transition(ExperimentState::Passed, "required checks passed");
    output.state = ExperimentState::Passed;
  }
  ReportInput report_input;
  report_input.manifest = &manifest;
  report_input.environment = environment;
  report_input.build = built.record;
  report_input.runs = runs;
  report_input.observations = observations;
  report_input.checks = checks;
  report_input.state = output.state;
  report_input.failure_class = failure_class;
  report_input.variant_build_type = variant.build_type;
  const std::string readme_path = join_path(manifest.directory, "README.md");
  const std::string behavior_path = join_path(manifest.directory, "expected/behavior.md");
  if (path_exists(readme_path)) {
    report_input.readme = read_file(readme_path);
  }
  if (path_exists(behavior_path)) {
    report_input.behavior = read_file(behavior_path);
  }
  write_file(join_path(run_directory, "report.md"), render_report(report_input));
  write_file(join_path(run_directory, "lifecycle.json"), lifecycle.history().dump());
  publish_latest(config.artifacts_dir, manifest.experiment.id, run_directory, built.record.build_id);
  output.failure_class = failure_class;
  if (output.state == ExperimentState::Passed) {
    output.message = "检查通过";
  } else if (output.state == ExperimentState::Failed) {
    output.message = "未通过";
  } else if (output.state == ExperimentState::Unsupported) {
    output.message = "本机不支持";
  } else if (output.state == ExperimentState::Interrupted) {
    output.message = "已中断";
  } else {
    output.message = to_string(output.state);
  }
  output.run_directory = run_directory;
  output.summary.set("state", Json::string(to_string(output.state)));
  output.summary.set("failure_class", Json::string(failure_class));
  output.summary.set("run_directory", Json::string(run_directory));
  output.summary.set("lifecycle", lifecycle.history());
  return output;
}

}  // namespace syslab
