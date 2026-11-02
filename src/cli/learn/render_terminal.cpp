#include "../learn.hpp"
#include "internal.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/experiment/model.hpp"
#include "syslab/observation/protocol.hpp"

#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace syslab {

const char* state_phrase(ExperimentState state) {
  switch (state) {
    case ExperimentState::Passed:
      return "通过";
    case ExperimentState::Failed:
      return "未通过";
    case ExperimentState::Unsupported:
      return "当前主机不支持";
    case ExperimentState::Interrupted:
      return "已中断";
    default:
      return "未完成";
  }
}

int status_code(ExperimentState state) {
  if (state == ExperimentState::Passed || state == ExperimentState::Built) {
    return 0;
  }
  if (state == ExperimentState::Unsupported) {
    return 3;
  }
  if (state == ExperimentState::Interrupted) {
    return 4;
  }
  return 2;
}

int worse_status(int left, int right) {
  auto rank = [](int code) {
    if (code == 2) {
      return 0;
    }
    if (code == 4) {
      return 1;
    }
    if (code == 3) {
      return 2;
    }
    if (code == 1) {
      return 3;
    }
    return 4;
  };
  return rank(left) <= rank(right) ? left : right;
}

const char* phrase_for_code(int status) {
  if (status == 0) {
    return "通过";
  }
  if (status == 3) {
    return "当前主机不支持";
  }
  if (status == 4) {
    return "已中断";
  }
  return "未通过";
}

void print_course(const std::vector<Lesson>& lessons) {
  std::cout << "共 " << lessons.size() << " 节。\n";
  std::cout << "打开    make web\n";
  std::cout << "接着    make learn N=next\n";
  std::cout << "页面    artifacts/lessons/index.html\n";
  const char* layers[] = {"基础", "进阶", "方向"};
  for (const char* layer : layers) {
    bool any = false;
    std::string group;
    for (std::size_t index = 0; index < lessons.size(); ++index) {
      const Lesson& lesson = lessons[index];
      if (lesson.layer != layer) {
        continue;
      }
      if (!any) {
        std::cout << "\n" << layer << "\n";
        any = true;
      }
      if (lesson.group != group) {
        group = lesson.group;
        std::cout << "  " << group << "\n";
      }
      std::cout << "    " << std::setw(2) << (index + 1) << "  " << lesson.title << "\n";
    }
  }
}

void print_terminal(const std::string& root, const std::vector<Lesson>& lessons, std::size_t index,
                    const Registry& registry, const std::map<std::string, std::string>& figures,
                    const LessonCopy& copy, const std::vector<RunView>& runs) {
  const Lesson& lesson = lessons[index];
  const Manifest& manifest = registry.get(lesson.id);
  const std::string readme = readme_text(manifest);
  const std::string checks = markdown_section(readme, "程序实际检查的内容");
  const std::string limit = without_runner_notice(markdown_section(readme, "不由本实验断言的内容"));
  std::cout << "\n第 " << (index + 1) << " 节 / " << lessons.size() << "\n";
  std::cout << lesson.title << "\n";
  std::cout << lesson.layer << " · " << lesson.group << "\n\n";
  const char* previous_label = nullptr;
  if (const Lesson* previous = track_neighbor(lessons, index, -1, &previous_label)) {
    std::cout << previous_label << "  " << previous->title << "\n";
    if (previous->group != lesson.group || previous->layer != lesson.layer) {
      std::cout << "  " << previous->layer << " · " << previous->group << "\n";
    }
  }
  std::cout << "这一事实  " << lesson.title << "\n";
  const char* next_label = nullptr;
  if (const Lesson* next = track_neighbor(lessons, index, 1, &next_label)) {
    std::cout << next_label << "  " << next->title << "\n";
    if (next->group != lesson.group || next->layer != lesson.layer) {
      std::cout << "  " << next->layer << " · " << next->group << "\n";
    }
  }
  std::cout << "\n";
  const auto brief = copy.briefs.find(lesson.id);
  const bool group_start = index == 0 || lessons[index - 1].group != lesson.group || lessons[index - 1].layer != lesson.layer;
  if (brief != copy.briefs.end() && !brief->second.focus.empty()) {
    std::cout << brief->second.focus << "\n\n";
  }
  if (brief != copy.briefs.end() && !brief->second.background.empty()) {
    std::cout << "背景\n" << brief->second.background << "\n\n";
  }
  const auto figure = figures.find(lesson.id);
  if (figure != figures.end() && !figure->second.empty()) {
    std::cout << figure->second << "\n\n";
  }
  if (brief != copy.briefs.end() && !brief->second.term.empty()) {
    std::cout << "名词\n" << brief->second.term << "\n\n";
  }
  if (group_start) {
    const auto group_text = copy.groups.find(lesson.group);
    if (group_text != copy.groups.end() && !group_text->second.empty()) {
      std::cout << "这一组\n" << group_text->second << "\n\n";
    }
  }
  if (!manifest.prerequisites.empty()) {
    std::cout << "先看";
    for (const std::string& required : manifest.prerequisites) {
      const Lesson* required_lesson = find_lesson(lessons, required);
      std::cout << " " << (required_lesson == nullptr ? required : required_lesson->title);
    }
    std::cout << "\n\n";
  }
  const bool show_fact = brief != copy.briefs.end() && !brief->second.principle.empty() &&
                         brief->second.principle != brief->second.mechanism;
  if (brief != copy.briefs.end() && !brief->second.language.empty()) {
    std::cout << "规则\n" << brief->second.language << "\n\n";
  }
  if (!show_fact && brief != copy.briefs.end()) {
    std::cout << "原理\n" << brief->second.mechanism << "\n\n";
  }
  std::cout << "先写下预期\n"
            << (brief != copy.briefs.end() && !brief->second.focus.empty() ? brief->second.focus
                                                                          : "按上面的规则写下本机应出现的读数。")
            << "\n\n";
  if (show_fact) {
    std::cout << "成立的事实\n" << brief->second.principle << "\n\n";
    std::cout << "机制\n" << brief->second.mechanism << "\n\n";
  }
  if (brief != copy.briefs.end() && !brief->second.program.empty()) {
    std::cout << "本节程序\n" << brief->second.program << "\n\n";
  }
  if (!show_fact) {
    std::cout << "程序要成立的事实\n" << (checks.empty() ? lesson.diagnose : checks) << "\n\n";
  }
  std::cout << "本机读数\n";
  for (const RunView& run : runs) {
    if (!run.label.empty()) {
      std::cout << run.label << " · " << state_phrase(run.state) << "\n";
    }
    if (!run.run_directory.empty()) {
      std::cout << readings_text(run.run_directory, false);
      if (lesson.disasm || lesson.both) {
        const std::string disassembly = join_path(run.run_directory, "disassembly.txt");
        if (path_exists(disassembly)) {
          std::cout << "  反汇编 " << display_path(root, disassembly) << "\n";
        }
      }
    }
  }
  std::cout << "\n出现同类故障时，先核对\n" << lesson.diagnose << "\n\n";
  if (!limit.empty()) {
    std::cout << "证明到哪一层\n" << limit << "\n\n";
  }
  std::cout << "源码 " << display_path(root, join_path(manifest.directory, "main.cpp")) << "\n";
  std::cout << "页面 artifacts/lessons/index.html#" << lesson.id << "\n";
  if (index + 1 < lessons.size()) {
    std::cout << "下一节 make learn N=" << (index + 2) << "\n";
  } else {
    std::cout << "这是最后一节。\n";
  }
}

int command_learn(const std::string& root, const std::string& argument, const SessionConfig& config, Registry& registry) {
  const std::vector<Lesson> lessons = load_course(root, registry);
  const std::map<std::string, std::string> figures = load_figures(root);
  const LessonCopy copy = load_briefs(root);
  if (argument.empty()) {
    write_course_page(root, lessons, registry, figures, copy, "", {}, "");
    print_course(lessons);
    std::vector<std::string> covered;
    covered.reserve(lessons.size());
    for (const Lesson& lesson : lessons) {
      covered.push_back(lesson.id);
    }
    for (const std::string& id : registry.ids()) {
      if (std::find(covered.begin(), covered.end(), id) == covered.end()) {
        std::cerr << "课程还没有收进：" << id << "\n";
      }
    }
    return 0;
  }
  const std::size_t index = select_lesson(root, argument, lessons);
  const Lesson& lesson = lessons[index];
  const Manifest& manifest = registry.get(lesson.id);
  std::vector<RunView> runs;
  SessionConfig run_config = config;
  run_config.variant.clear();
  const SessionOutput first = execute_session(manifest, run_config);
  runs.push_back(RunView{lesson.both ? "未优化" : "", first.run_directory, first.state});
  int status = status_code(first.state);
  if (lesson.both) {
    std::cout << "同一份源码再用优化构建跑一次。检查相同，只说明抽象机结果还在。\n";
    run_config.variant = "optimized";
    const SessionOutput second = execute_session(manifest, run_config);
    runs.push_back(RunView{"优化后", second.run_directory, second.state});
    status = worse_status(status, status_code(second.state));
  }
  write_course_page(root, lessons, registry, figures, copy, lesson.id, runs, "");
  print_terminal(root, lessons, index, registry, figures, copy, runs);
  std::cout << "结果：" << phrase_for_code(status) << "\n";
  remember_next(root, index, lessons.size(), status);
  return status;
}

}  // namespace syslab
