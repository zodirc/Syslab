#include "syslab/experiment/session.hpp"

#include "syslab/build/build_manager.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/time.hpp"
#include "syslab/experiment/lifecycle.hpp"
#include "syslab/observation/elf.hpp"
#include "syslab/observation/report.hpp"
#include "syslab/platform/capability.hpp"
#include "syslab/platform/environment.hpp"
#include "syslab/runner/process.hpp"

#include <sys/utsname.h>

#include <algorithm>
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

std::string tool_path(const char* name) {
  const std::string path = std::string("/usr/bin/") + name;
  return path_exists(path) ? path : std::string();
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

std::string result_status(const std::string& stdout_text) {
  const std::string marker = "SYSLAB_RESULT";
  std::size_t cursor = 0;
  while (cursor < stdout_text.size()) {
    const std::size_t end = stdout_text.find('\n', cursor);
    const std::string line = stdout_text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (line.compare(0, marker.size(), marker) == 0) {
      const std::size_t key = line.find("status=");
      if (key != std::string::npos) {
        const std::size_t value = key + 7;
        const std::size_t stop = line.find(' ', value);
        return line.substr(value, stop == std::string::npos ? std::string::npos : stop - value);
      }
    }
    if (end == std::string::npos) {
      break;
    }
    cursor = end + 1;
  }
  return {};
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
    result.actual = result_status(stdout_text);
    if (result.actual.empty()) {
      result.actual = "missing";
      result.status = VerificationStatus::Failed;
      result.explanation = "SYSLAB_RESULT marker was not printed";
    } else if (result.actual == "unsupported") {
      result.status = VerificationStatus::Unsupported;
      result.explanation = "experiment reported that the host cannot perform the observation";
    } else if (result.actual == spec.expected) {
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
                                   const std::string& limitation) {
  ObservationRecord record;
  record.observation_id = make_id("obs");
  record.run_id = run_id;
  record.kind = kind;
  record.tool_name = tool;
  record.tool_version = version;
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

void publish_latest(const std::string& artifacts, const std::string& id, const std::string& directory) {
  const std::string latest = join_path(artifacts, "latest");
  create_directories(latest);
  write_file(join_path(latest, id + ".txt"), directory + "\n");
  const std::string line = std::string("{\"experiment_id\":\"") + id + "\",\"directory\":\"" + directory + "\"}\n";
  const std::string index_path = join_path(artifacts, "index.jsonl");
  std::string existing;
  if (path_exists(index_path)) {
    existing = read_file(index_path);
  }
  existing += line;
  write_file(index_path, existing);
}

}  // namespace

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

SessionOutput execute_session(const Manifest& manifest, const SessionConfig& config) {
  Lifecycle lifecycle(manifest.experiment.id);
  SessionOutput output;
  output.summary = manifest_to_json(manifest);
  lifecycle.transition(ExperimentState::Validating, "manifest loaded by the registry");
  const std::string refusal = refusal_reason(manifest);
  lifecycle.transition(ExperimentState::EnvironmentCheck, refusal.empty() ? "host matches the manifest" : refusal);
  if (!refusal.empty()) {
    lifecycle.transition(ExperimentState::Unsupported, refusal);
    output.state = ExperimentState::Unsupported;
    output.failure_class = "unsupported";
    output.message = refusal;
    output.summary.set("state", Json::string(to_string(output.state)));
    output.summary.set("failure_class", Json::string(output.failure_class));
    output.summary.set("message", Json::string(output.message));
    output.summary.set("lifecycle", lifecycle.history());
    return output;
  }
  lifecycle.transition(ExperimentState::Building, "compiling the selected variant");
  BuildContext context;
  context.source_root = config.source_root;
  context.compiler = config.compiler;
  context.library_dir = config.library_dir;
  context.polyfill_dir = config.polyfill_dir;
  context.output_directory = join_path(config.artifacts_dir, "builds");
  BuiltExperiment built = build_experiment(manifest, find_variant(manifest, config.variant), context);
  if (!built.success) {
    lifecycle.transition(ExperimentState::Failed, built.message);
    output.state = ExperimentState::Failed;
    output.failure_class = "build";
    output.message = built.message;
    output.run_directory = built.directory;
    output.summary.set("state", Json::string(to_string(output.state)));
    output.summary.set("failure_class", Json::string(output.failure_class));
    output.summary.set("message", Json::string(output.message));
    output.summary.set("lifecycle", lifecycle.history());
    return output;
  }
  lifecycle.transition(ExperimentState::Built, built.record.build_id);
  if (interrupt_requested()) {
    lifecycle.transition(ExperimentState::Interrupted, "interrupt requested before running");
    output.state = ExperimentState::Interrupted;
    output.failure_class = "interrupted";
    output.message = "interrupted";
    output.summary.set("state", Json::string(to_string(output.state)));
    output.summary.set("lifecycle", lifecycle.history());
    return output;
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
  const std::string strace = tool_path("strace");
  const bool want_strace = [&]() {
    for (const std::string& item : manifest.observe) {
      if (parse_observation_kind(item) == ObservationKind::SyscallTrace) {
        return true;
      }
    }
    return false;
  }();
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
    if (want_strace && !strace.empty()) {
      request.argv = {strace, "-f", "-s", "80", "-o", join_path(run_directory, run.run_id + ".strace"), "--",
                      built.executable};
    } else {
      request.argv = {built.executable};
    }
    request.argv.insert(request.argv.end(), manifest.arguments.begin(), manifest.arguments.end());
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
    if (!result.spawn_error.empty()) {
      write_file(stderr_path, result.spawn_error + "\n");
    }
    write_file(join_path(run_directory, run.run_id + ".json"), run_to_json(run).dump());
    stdouts.push_back(path_exists(stdout_path) ? read_file(stdout_path) : result.stdout_text);
    stderrs.push_back(path_exists(stderr_path) ? read_file(stderr_path) : result.stderr_text);
    runs.push_back(std::move(run));
  }
  if (runs.empty() && interrupt_requested()) {
    lifecycle.transition(ExperimentState::Interrupted, "interrupt requested before a run completed");
    output.state = ExperimentState::Interrupted;
    output.failure_class = "interrupted";
    output.message = "interrupted";
    output.run_directory = run_directory;
    output.summary.set("state", Json::string(to_string(output.state)));
    output.summary.set("lifecycle", lifecycle.history());
    return output;
  }
  lifecycle.transition(ExperimentState::Observing, "collecting stdout, stderr, and optional tools");
  std::vector<ObservationRecord> observations;
  if (!runs.empty()) {
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::StandardOutput, "runner", "syslab",
                                            runs.back().stdout_artifact, true, ""));
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::StandardError, "runner", "syslab",
                                            runs.back().stderr_artifact, true, ""));
    observations.push_back(file_observation(runs.back().run_id, ObservationKind::ExitStatus, "runner", "syslab",
                                            join_path(run_directory, runs.back().run_id + ".json"), true, ""));
    for (const std::string& item : manifest.observe) {
      const ObservationKind kind = parse_observation_kind(item);
      if (kind == ObservationKind::Disassembly) {
        const std::string objdump = tool_path("objdump");
        const std::string path = join_path(run_directory, "disassembly.txt");
        if (objdump.empty()) {
          observations.push_back(file_observation(runs.back().run_id, kind, "objdump", "unavailable", path, false,
                                                  "objdump is not installed at /usr/bin/objdump"));
        } else {
          CommandRequest request;
          request.argv = {objdump, "-d", "--no-show-raw-insn", built.executable};
          request.inherit_environment = true;
          request.timeout_ms = 20000;
          request.stdout_path = path;
          request.output_limit = 8 * 1024 * 1024;
          const CommandResult result = run_command(request);
          observations.push_back(file_observation(runs.back().run_id, kind, "objdump", "binutils", path,
                                                  result.exit_code == 0, result.exit_code == 0 ? "" : "objdump failed"));
        }
      } else if (kind == ObservationKind::ElfMetadata) {
        const ElfImage image = read_elf(built.executable);
        const std::string path = join_path(run_directory, "elf.json");
        write_file(path, elf_to_json(image).dump());
        observations.push_back(file_observation(runs.back().run_id, kind, "syslab-elf", "1", path, image.supported,
                                                image.limitation));
      } else if (kind == ObservationKind::SyscallTrace) {
        const std::string path = join_path(run_directory, runs.back().run_id + ".strace");
        if (strace.empty()) {
          observations.push_back(file_observation(runs.back().run_id, kind, "strace", "unavailable", path, false,
                                                  "strace is not installed at /usr/bin/strace"));
        } else {
          observations.push_back(
              file_observation(runs.back().run_id, kind, "strace", "strace", path, path_exists(path),
                               path_exists(path) ? "" : "strace did not produce a trace"));
        }
      } else if (kind == ObservationKind::CompilerMetadata) {
        const std::string path = join_path(built.directory, "build.json");
        observations.push_back(file_observation(runs.back().run_id, kind, "compiler", built.record.compiler_version,
                                                path, path_exists(path),
                                                path_exists(path) ? "" : "build.json was not written"));
      } else if (kind == ObservationKind::PerformanceCounter) {
        const std::string perf = tool_path("perf");
        observations.push_back(file_observation(
            runs.back().run_id, kind, "perf", perf.empty() ? "unavailable" : "perf",
            join_path(run_directory, "perf.txt"), false,
            perf.empty() ? "perf is not installed at /usr/bin/perf"
                         : "performance counters were not sampled for this run"));
      } else if (kind == ObservationKind::StandardOutput || kind == ObservationKind::StandardError ||
                 kind == ObservationKind::ExitStatus) {
      } else {
        observations.push_back(file_observation(
            runs.back().run_id, kind, "syslab", "unavailable", join_path(run_directory, "unavailable.txt"), false,
            std::string(to_string(kind)) + " was not sampled during this run"));
      }
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
  publish_latest(config.artifacts_dir, manifest.experiment.id, run_directory);
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
