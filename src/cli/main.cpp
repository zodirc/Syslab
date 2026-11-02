#include "learn.hpp"
#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/json.hpp"
#include "syslab/experiment/catalog.hpp"
#include "syslab/experiment/registry.hpp"
#include "syslab/experiment/session.hpp"
#include "syslab/observation/elf.hpp"
#include "syslab/platform/capability.hpp"
#include "syslab/platform/cpu.hpp"
#include "syslab/platform/environment.hpp"
#include "syslab/platform/toolchain.hpp"
#include "syslab/runtime/layout.hpp"

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string root;
  std::string format = "text";
  std::string variant;
  std::string module;
  std::string artifact;
  std::string binary;
  std::string syntax = "att";
  int repeat = -1;
  bool capabilities = false;
  bool latest = false;
  bool all = false;
  bool use_cache = true;
  int keep = -1;
};

std::string executable_directory() {
  char buffer[4096];
  const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length < 0) {
    return ".";
  }
  buffer[length] = '\0';
  return syslab::parent_path(buffer);
}

std::string default_root() {
#ifdef SYSLAB_SOURCE_DIR
  return SYSLAB_SOURCE_DIR;
#else
  return ".";
#endif
}

#ifdef SYSLAB_COMPILER
const char* kCompiler = SYSLAB_COMPILER;
#else
const char* kCompiler = "/usr/bin/clang++";
#endif

#ifdef SYSLAB_POLYFILL_DIR
const char* kPolyfill = SYSLAB_POLYFILL_DIR;
#else
const char* kPolyfill = "";
#endif

void print_usage() {
  std::cerr
      << "用法（在仓库根目录从 make learn 开始）:\n"
      << "  syslab learn\n"
      << "      列出课程。图文页写到 artifacts/lessons/index.html。\n"
      << "  syslab learn 课次|next|编号\n"
      << "      上某一课：先给结构图和本机读数，并写明排查时先核对的事实。\n"
      << "  syslab web [端口]\n"
      << "      在 http://127.0.0.1:9527/ 打开页面。端口可改。Ctrl+C 停止。\n"
      << "  syslab list [--module 模块名] [--format text|json]\n"
      << "      列出实验。text 格式是：编号、名称、模块、风险。\n"
      << "  syslab show 编号\n"
      << "      查看一份清单的摘要。\n"
      << "  syslab validate --all\n"
      << "      检查实验目录、索引和课程标识是否一致。\n"
      << "  syslab validate 编号\n"
      << "      只检查清单和本机能力，不编译。\n"
      << "  syslab build 编号 [--variant debug|optimized] [--no-cache]\n"
      << "      编译一个变体。默认用清单里的第一个变体。--no-cache 强制重新编译。\n"
      << "  syslab run 编号 [--variant 名称] [--repeat 次数] [--no-cache]\n"
      << "      编译、运行、取证、校验，并写报告。--no-cache 强制重新编译。\n"
      << "  syslab report 编号 --latest\n"
      << "      打印该实验最近一次的 report.md。\n"
      << "  syslab inspect 编号 --artifact disassembly|report|elf\n"
      << "      打印最近一次运行的反汇编、报告或 ELF 记录。\n"
      << "  syslab inspect asm --binary 路径 [--syntax att|intel]\n"
      << "  syslab inspect elf --binary 路径\n"
      << "  syslab inspect layout\n"
      << "  syslab env [--capabilities]\n"
      << "      环境快照。--capabilities 查看 objdump、perf 等是否可用。\n"
      << "  syslab clean --keep 数量\n"
      << "      每个实验保留最近的若干次运行，以及这些运行引用的构建。\n"
      << "  syslab toolchain list\n"
      << "  syslab toolchain inspect gcc|g++|clang|clang++\n"
      << "\n"
      << "退出码: 0 通过或构建完成，2 失败，3 本机不支持，4 中断，1 用法或内部错误。\n"
      << "kernel_privileged 和 bare_metal 不能在日常主机上运行。\n";
}

bool take_value(int* index, int argc, char** argv, std::string* value) {
  if (*index + 1 >= argc) {
    return false;
  }
  *value = argv[++(*index)];
  return true;
}

syslab::SessionConfig make_config(const Options& options) {
  syslab::SessionConfig config;
  config.source_root = options.root;
  config.compiler = kCompiler;
  config.library_dir = executable_directory();
  config.polyfill_dir = kPolyfill;
  config.artifacts_dir = syslab::join_path(options.root, "artifacts");
  config.variant = options.variant;
  config.repeat = options.repeat;
  config.use_cache = options.use_cache;
  return config;
}

std::string latest_directory(const Options& options, const std::string& id) {
  const std::string path = syslab::join_path(syslab::join_path(options.root, "artifacts/latest"), id + ".txt");
  if (!syslab::path_exists(path)) {
    throw syslab::Error(syslab::Error::Code::NotFound, "no recorded run for " + id);
  }
  std::string directory = syslab::read_file(path);
  while (!directory.empty() && (directory.back() == '\n' || directory.back() == '\r')) {
    directory.pop_back();
  }
  return directory;
}

std::string trim_copy(std::string text) {
  std::size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\n' || text[begin] == '\r' || text[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\n' || text[end - 1] == '\r' || text[end - 1] == '\t')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string markdown_section(const std::string& text, const char* heading) {
  const std::string marker = std::string("## ") + heading;
  std::size_t start = 0;
  while (start < text.size()) {
    if ((start == 0 || text[start - 1] == '\n') && text.compare(start, marker.size(), marker) == 0 &&
        (start + marker.size() == text.size() || text[start + marker.size()] == '\n' ||
         text[start + marker.size()] == '\r')) {
      break;
    }
    const std::size_t next = text.find('\n', start);
    if (next == std::string::npos) {
      return {};
    }
    start = next + 1;
  }
  if (start >= text.size()) {
    return {};
  }
  const std::size_t body = text.find('\n', start);
  if (body == std::string::npos) {
    return {};
  }
  const std::size_t next_heading = text.find("\n## ", body + 1);
  const std::string section =
      text.substr(body + 1, next_heading == std::string::npos ? std::string::npos : next_heading - (body + 1));
  return trim_copy(section);
}

std::string without_runner_notice(const std::string& text) {
  const std::string notice = "反汇编、系统调用跟踪";
  std::string kept;
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find("\n\n", cursor);
    const std::string paragraph =
        text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (paragraph.compare(0, notice.size(), notice) != 0) {
      if (!kept.empty()) {
        kept += "\n\n";
      }
      kept += paragraph;
    }
    if (end == std::string::npos) {
      break;
    }
    cursor = end + 2;
  }
  return trim_copy(kept);
}

void print_block(const char* title, const std::string& body) {
  std::cout << title << "\n";
  if (body.empty()) {
    std::cout << "（说明文件里没有这一节）\n\n";
    return;
  }
  std::cout << body << "\n\n";
}

void print_protocol_line(const std::string& line) {
  constexpr const char* kMetric = "SYSLAB_METRIC ";
  constexpr const char* kNote = "SYSLAB_NOTE ";
  constexpr const char* kStep = "SYSLAB_STEP ";
  constexpr const char* kResult = "SYSLAB_RESULT ";
  if (line.compare(0, 12, kStep) == 0) {
    std::cout << "  观测 " << line.substr(12) << "\n";
    return;
  }
  if (line.compare(0, 14, kMetric) == 0) {
    std::cout << "  度量 " << line.substr(14) << "\n";
    return;
  }
  if (line.compare(0, 12, kNote) == 0) {
    const std::string body = line.substr(12);
    const std::size_t text_key = body.find(" text=");
    if (body.compare(0, 6, "basis=") == 0 && text_key != std::string::npos) {
      std::cout << "  依据 " << body.substr(6, text_key - 6) << "：" << body.substr(text_key + 6) << "\n";
      return;
    }
  }
  if (line.compare(0, 14, kResult) == 0) {
    std::cout << "  程序标记 " << line.substr(14) << "\n";
    return;
  }
  std::cout << "  " << line << "\n";
}

void print_observations(const std::string& run_directory) {
  std::vector<std::string> files;
  DIR* directory = ::opendir(run_directory.c_str());
  if (directory != nullptr) {
    while (const dirent* entry = ::readdir(directory)) {
      const std::string name = entry->d_name;
      if (name.size() >= 7 && name.compare(name.size() - 7, 7, ".stdout") == 0) {
        files.push_back(syslab::join_path(run_directory, name));
      }
    }
    ::closedir(directory);
  }
  std::sort(files.begin(), files.end());
  std::cout << "这次运行里，程序自己打印的内容\n";
  if (files.empty()) {
    std::cout << "没有读到标准输出。\n\n";
    return;
  }
  for (std::size_t index = 0; index < files.size(); ++index) {
    if (files.size() > 1) {
      std::cout << "第 " << (index + 1) << " 次\n";
    }
    const std::string text = syslab::read_file(files[index]);
    std::size_t cursor = 0;
    bool any = false;
    while (cursor < text.size()) {
      const std::size_t end = text.find('\n', cursor);
      const std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
      if (!line.empty() && line.back() != '\r') {
        print_protocol_line(line);
        any = true;
      } else if (line.size() > 1) {
        print_protocol_line(line.substr(0, line.size() - 1));
        any = true;
      }
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 1;
    }
    if (!any) {
      std::cout << "  （这次标准输出是空的）\n";
    }
  }
  std::cout << "\n";
}

void print_lesson(const syslab::Manifest& manifest, const std::string& run_directory) {
  const std::string readme_path = syslab::join_path(manifest.directory, "README.md");
  const std::string readme = syslab::path_exists(readme_path) ? syslab::read_file(readme_path) : std::string();
  std::string purpose = markdown_section(readme, "实验目的");
  if (purpose.empty()) {
    purpose = manifest.experiment.description;
  }
  std::cout << manifest.experiment.id << "  " << manifest.experiment.name << "\n";
  std::cout << "模块 " << manifest.experiment.module << "\n\n";
  print_block("要弄清的问题", purpose);
  if (!run_directory.empty()) {
    print_observations(run_directory);
  }
  print_block("程序在检查什么", markdown_section(readme, "程序实际检查的内容"));
  print_block("这些结论不能从这次检查推出来",
              without_runner_notice(markdown_section(readme, "不由本实验断言的内容")));
  std::cout << "源码目录: " << manifest.directory << "\n";
  if (!run_directory.empty()) {
    std::cout << "完整报告: " << syslab::join_path(run_directory, "report.md") << "\n";
  }
}

int print_session(const Options& options, const syslab::SessionOutput& output) {
  if (options.format == "json") {
    std::cout << output.summary.dump();
  } else {
    std::cout << syslab::to_string(output.state) << " " << output.message << "\n";
    if (!output.run_directory.empty()) {
      std::cout << output.run_directory << "\n";
    }
  }
  if (output.state == syslab::ExperimentState::Passed || output.state == syslab::ExperimentState::Built ||
      output.state == syslab::ExperimentState::Unsupported) {
    return output.state == syslab::ExperimentState::Unsupported ? 3 : 0;
  }
  if (output.state == syslab::ExperimentState::Interrupted) {
    return 4;
  }
  return 2;
}

struct StandardLayoutSample {
  std::uint32_t first;
  std::uint8_t second;
  std::uint64_t third;
};

void print_layout() {
  const syslab::HostArchitecture architecture = syslab::host_architecture();
  std::cout << "arch=" << architecture.name << " ip=" << architecture.instruction_pointer
            << " sp=" << architecture.stack_pointer << " return=" << architecture.integer_return_register
            << " syscall=" << architecture.syscall_instruction << " memory_model=" << architecture.memory_model
            << "\n";
  std::cout << "basis=ABI layout below is for this compiler and ABI\n";
  syslab::write_layout<StandardLayoutSample>(
      std::cout, "StandardLayoutSample",
      {SYSLAB_FIELD(StandardLayoutSample, first), SYSLAB_FIELD(StandardLayoutSample, second),
       SYSLAB_FIELD(StandardLayoutSample, third)});
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      print_usage();
      return 1;
    }
    Options options;
    options.root = default_root();
    std::vector<std::string> positionals;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--help" || argument == "-h") {
        print_usage();
        return 0;
      }
      if (argument == "--format") {
        if (!take_value(&index, argc, argv, &options.format) ||
            (options.format != "text" && options.format != "json")) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--format requires text or json");
        }
      } else if (argument == "--root") {
        if (!take_value(&index, argc, argv, &options.root)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--root requires a path");
        }
      } else if (argument == "--module") {
        if (!take_value(&index, argc, argv, &options.module)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--module requires a name");
        }
      } else if (argument == "--variant") {
        if (!take_value(&index, argc, argv, &options.variant)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--variant requires a name");
        }
      } else if (argument == "--repeat") {
        std::string text;
        if (!take_value(&index, argc, argv, &text)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--repeat requires a number");
        }
        options.repeat = std::stoi(text);
        if (options.repeat < 1) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--repeat must be positive");
        }
      } else if (argument == "--artifact") {
        if (!take_value(&index, argc, argv, &options.artifact)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--artifact requires a name");
        }
      } else if (argument == "--binary") {
        if (!take_value(&index, argc, argv, &options.binary)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--binary requires a path");
        }
      } else if (argument == "--syntax") {
        if (!take_value(&index, argc, argv, &options.syntax)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--syntax requires att or intel");
        }
      } else if (argument == "--capabilities") {
        options.capabilities = true;
      } else if (argument == "--latest") {
        options.latest = true;
      } else if (argument == "--all") {
        options.all = true;
      } else if (argument == "--no-cache") {
        options.use_cache = false;
      } else if (argument == "--keep") {
        std::string text;
        if (!take_value(&index, argc, argv, &text)) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--keep requires a number");
        }
        try {
          options.keep = std::stoi(text);
        } catch (const std::exception&) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--keep requires a number");
        }
        if (options.keep < 0) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "--keep must be zero or positive");
        }
      } else if (!argument.empty() && argument[0] == '-') {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "unknown option " + argument);
      } else {
        positionals.push_back(argument);
      }
    }
    if (positionals.empty()) {
      print_usage();
      return 1;
    }
    syslab::Registry registry;
    const auto load = [&]() { registry.load_tree(syslab::join_path(options.root, "experiments")); };
    const std::string command = positionals[0];
    if (command == "clean") {
      if (positionals.size() != 1) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "clean does not take an experiment id");
      }
      if (options.keep < 0) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "clean requires --keep N");
      }
      const syslab::CleanReport report =
          syslab::clean_artifacts(syslab::join_path(options.root, "artifacts"), static_cast<std::size_t>(options.keep));
      std::cout << "removed runs=" << report.runs_removed << " builds=" << report.builds_removed
                << " cache=" << report.cache_removed << "\n";
      return 0;
    }
    if (command == "list") {
      load();
      const std::vector<std::string> ids = registry.ids_for_module(options.module);
      if (options.format == "json") {
        syslab::Json array = syslab::Json::array();
        for (const std::string& id : ids) {
          array.push(syslab::manifest_to_json(registry.get(id)));
        }
        syslab::Json json = syslab::Json::object();
        json.set("experiments", std::move(array));
        std::cout << json.dump();
      } else {
        for (const std::string& id : ids) {
          const syslab::Manifest& manifest = registry.get(id);
          std::cout << manifest.experiment.id << "\t" << manifest.experiment.name << "\t"
                    << manifest.experiment.module << "\t" << syslab::to_string(manifest.experiment.risk) << "\n";
        }
      }
      return 0;
    }
    if (command == "env") {
      if (options.capabilities) {
        const auto capabilities = syslab::probe_capabilities();
        if (options.format == "json") {
          std::cout << syslab::capabilities_to_json(capabilities).dump();
        } else {
          for (const syslab::Capability& capability : capabilities) {
            std::cout << capability.name << "\t" << capability.status << "\t" << capability.version;
            if (!capability.reason.empty()) {
              std::cout << "\t" << capability.reason;
            }
            std::cout << "\n";
          }
        }
      } else {
        const syslab::EnvironmentSnapshot snapshot = syslab::capture_environment(kCompiler);
        if (options.format == "json") {
          std::cout << syslab::environment_to_json(snapshot).dump();
        } else {
          std::cout << syslab::environment_to_json(snapshot).dump();
        }
      }
      return 0;
    }
    if (command == "toolchain") {
      if (positionals.size() < 2) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "toolchain requires list or inspect");
      }
      if (positionals[1] == "list") {
        const auto toolchains = syslab::list_toolchains();
        std::cout << (options.format == "json" ? syslab::toolchains_to_json(toolchains).dump()
                                               : syslab::toolchains_to_json(toolchains).dump());
        return 0;
      }
      if (positionals[1] == "inspect" && positionals.size() >= 3) {
        const syslab::Toolchain toolchain = syslab::inspect_toolchain(positionals[2]);
        std::cout << syslab::toolchains_to_json({toolchain}).dump();
        return toolchain.available ? 0 : 3;
      }
      throw syslab::Error(syslab::Error::Code::InvalidArgument, "unknown toolchain command");
    }
    if (command == "inspect" && positionals.size() >= 2 && positionals[1] == "layout") {
      print_layout();
      return 0;
    }
    if (command == "inspect" && positionals.size() >= 2 && positionals[1] == "elf") {
      if (options.binary.empty()) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "inspect elf requires --binary");
      }
      std::cout << syslab::elf_to_json(syslab::read_elf(options.binary)).dump();
      return 0;
    }
    if (command == "inspect" && positionals.size() >= 2 && positionals[1] == "asm") {
      if (options.binary.empty()) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "inspect asm requires --binary");
      }
      const std::string objdump = "/usr/bin/objdump";
      if (!syslab::path_exists(objdump)) {
        throw syslab::Error(syslab::Error::Code::Unsupported, "objdump is unavailable");
      }
      std::vector<std::string> args = {objdump, "-d", "--no-show-raw-insn"};
      if (options.syntax == "intel") {
        args.push_back("-M");
        args.push_back("intel");
      }
      args.push_back(options.binary);
      std::vector<char*> pointers;
      std::vector<std::string> storage = args;
      for (std::string& item : storage) {
        pointers.push_back(item.data());
      }
      pointers.push_back(nullptr);
      execv(objdump.c_str(), pointers.data());
      throw syslab::Error(syslab::Error::Code::Execution, "failed to execute objdump");
    }
    if (command == "learn") {
      load();
      const std::string argument = positionals.size() >= 2 ? positionals[1] : std::string();
      return syslab::command_learn(options.root, argument, make_config(options), registry);
    }
    if (command == "web") {
      load();
      int port = 9527;
      if (positionals.size() >= 2) {
        try {
          port = std::stoi(positionals[1]);
        } catch (const std::exception&) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "端口应为 1 到 65535 的整数");
        }
      }
      return syslab::command_web(options.root, port, make_config(options), registry);
    }
    if (command == "validate" && options.all) {
      if (positionals.size() > 1) {
        throw syslab::Error(syslab::Error::Code::InvalidArgument, "validate --all does not take an experiment id");
      }
      load();
      const std::vector<std::string> issues = syslab::check_catalog(options.root, registry);
      if (options.format == "json") {
        syslab::Json summary = syslab::Json::object();
        summary.set("state", syslab::Json::string(issues.empty() ? "PASSED" : "FAILED"));
        syslab::Json array = syslab::Json::array();
        for (const std::string& issue : issues) {
          array.push(syslab::Json::string(issue));
        }
        summary.set("issues", std::move(array));
        std::cout << summary.dump();
      } else if (issues.empty()) {
        std::cout << "PASSED 实验目录、索引和课程标识一致\n";
      } else {
        std::cout << "FAILED\n";
        for (const std::string& issue : issues) {
          std::cout << "- " << issue << "\n";
        }
      }
      return issues.empty() ? 0 : 2;
    }
    if (positionals.size() < 2 && command != "list" && command != "env") {
      throw syslab::Error(syslab::Error::Code::InvalidArgument, command + " requires an experiment id");
    }
    load();
    if (command == "show" || command == "validate" || command == "build" || command == "run" ||
        command == "report" || (command == "inspect" && positionals.size() >= 2)) {
      const std::string id = positionals[1];
      const syslab::Manifest& manifest = registry.get(id);
      const syslab::SessionConfig config = make_config(options);
      if (command == "show") {
        if (options.format == "json") {
          std::cout << syslab::manifest_to_json(manifest).dump();
        } else {
          print_lesson(manifest, "");
          std::cout << "运行后，终端会列出程序打印的度量和依据，再给出是否通过。\n";
        }
        return 0;
      }
      if (command == "validate") {
        return print_session(options, syslab::validate_session(manifest, config));
      }
      if (command == "build") {
        return print_session(options, syslab::build_session(manifest, config));
      }
      if (command == "run") {
        const syslab::SessionOutput output = syslab::execute_session(manifest, config);
        if (options.format != "json") {
          print_lesson(manifest, output.run_directory);
          std::cout << "\n";
        }
        return print_session(options, output);
      }
      if (command == "report" || command == "inspect") {
        if (command == "report" && !options.latest) {
          throw syslab::Error(syslab::Error::Code::InvalidArgument, "report requires --latest");
        }
        const std::string directory = latest_directory(options, id);
        std::string filename = "report.md";
        if (command == "inspect") {
          if (options.artifact == "disassembly") {
            filename = "disassembly.txt";
          } else if (options.artifact == "elf") {
            filename = "elf.json";
          } else if (options.artifact == "report" || options.artifact.empty()) {
            filename = "report.md";
          } else {
            throw syslab::Error(syslab::Error::Code::InvalidArgument, "unknown artifact " + options.artifact);
          }
        }
        const std::string path = syslab::join_path(directory, filename);
        if (!syslab::path_exists(path)) {
          throw syslab::Error(syslab::Error::Code::NotFound, "artifact is absent: " + filename);
        }
        std::cout << syslab::read_file(path);
        return 0;
      }
    }
    print_usage();
    return 1;
  } catch (const syslab::Error& error) {
    std::cerr << syslab::to_string(error.code()) << ": " << error.what() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "execution: " << error.what() << "\n";
    return 1;
  }
}
