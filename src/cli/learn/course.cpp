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

std::string trim_copy(std::string text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::vector<std::string> split_fields(const std::string& line) {
  std::vector<std::string> fields;
  const std::string separator = " | ";
  std::size_t cursor = 0;
  while (cursor <= line.size()) {
    const std::size_t at = line.find(separator, cursor);
    if (at == std::string::npos) {
      fields.push_back(trim_copy(line.substr(cursor)));
      break;
    }
    fields.push_back(trim_copy(line.substr(cursor, at - cursor)));
    cursor = at + separator.size();
  }
  return fields;
}

std::vector<Lesson> load_course(const std::string& root, const Registry& registry) {
  const std::string path = join_path(root, "docs/learn/course.txt");
  if (!path_exists(path)) {
    throw Error(Error::Code::NotFound, "找不到课程：" + path);
  }
  std::vector<Lesson> lessons;
  std::string group;
  const std::string text = read_file(path);
  std::size_t cursor = 0;
  int line_number = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    ++line_number;
    cursor = end == std::string::npos ? text.size() : end + 1;
    if (trim_copy(line).empty()) {
      continue;
    }
    if (line[0] == '#') {
      group = trim_copy(line.substr(1));
      continue;
    }
    const std::vector<std::string> fields = split_fields(line);
    if (fields.size() < 3 || fields[0].empty() || fields[1].empty() || fields[2].empty()) {
      throw Error(Error::Code::Parse, "课程第 " + std::to_string(line_number) + " 行缺少编号、标题或排查说明");
    }
    if (!registry.contains(fields[0])) {
      throw Error(Error::Code::NotFound, "课程里的实验不存在：" + fields[0]);
    }
    Lesson lesson;
    lesson.group = group;
    lesson.id = fields[0];
    lesson.title = fields[1];
    lesson.diagnose = fields[2];
    if (fields.size() >= 4) {
      lesson.both = fields[3].find("both") != std::string::npos;
      lesson.disasm = fields[3].find("disasm") != std::string::npos;
      if (fields[3].find("base") != std::string::npos) {
        lesson.layer = "基础";
      }
      if (fields[3].find("direction") != std::string::npos) {
        lesson.layer = "方向";
      }
    }
    lessons.push_back(std::move(lesson));
  }
  if (lessons.empty()) {
    throw Error(Error::Code::Parse, "课程是空的");
  }
  return lessons;
}

std::map<std::string, std::string> load_figures(const std::string& root) {
  const std::string path = join_path(root, "docs/learn/figures.txt");
  std::map<std::string, std::string> figures;
  if (!path_exists(path)) {
    return figures;
  }
  const std::string text = read_file(path);
  std::string current_id;
  std::string body;
  auto flush = [&]() {
    if (!current_id.empty()) {
      figures[current_id] = trim_copy(body);
    }
  };
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    cursor = end == std::string::npos ? text.size() : end + 1;
    if (line.compare(0, 4, "=== ") == 0) {
      flush();
      current_id = trim_copy(line.substr(4));
      body.clear();
      continue;
    }
    if (!current_id.empty()) {
      body += line;
      body += '\n';
    }
  }
  flush();
  return figures;
}

LessonCopy load_briefs(const std::string& root) {
  LessonCopy copy;
  const std::string path = join_path(root, "docs/learn/brief.txt");
  if (!path_exists(path)) {
    return copy;
  }
  const std::string text = read_file(path);
  std::string group;
  std::string group_body;
  std::string current_id;
  LessonBrief brief;
  std::string* field = nullptr;
  auto flush_group = [&]() {
    if (!group.empty()) {
      copy.groups[group] = trim_copy(group_body);
    }
  };
  auto flush_brief = [&]() {
    if (!current_id.empty()) {
      brief.term = trim_copy(brief.term);
      brief.focus = trim_copy(brief.focus);
      brief.background = trim_copy(brief.background);
      brief.language = trim_copy(brief.language);
      brief.mechanism = trim_copy(brief.mechanism);
      brief.program = trim_copy(brief.program);
      brief.principle = trim_copy(brief.principle);
      if (brief.mechanism.empty()) {
        brief.mechanism = brief.principle;
      }
      copy.briefs[current_id] = std::move(brief);
      brief = LessonBrief{};
    }
  };
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    cursor = end == std::string::npos ? text.size() : end + 1;
    if (!line.empty() && line[0] == '#') {
      flush_brief();
      current_id.clear();
      field = nullptr;
      flush_group();
      group = trim_copy(line.substr(1));
      group_body.clear();
      continue;
    }
    if (line.compare(0, 4, "=== ") == 0) {
      flush_brief();
      current_id = trim_copy(line.substr(4));
      field = nullptr;
      continue;
    }
    if (current_id.empty()) {
      group_body += line;
      group_body += '\n';
      continue;
    }
    if (line == "名词") {
      field = &brief.term;
      continue;
    }
    if (line == "重点") {
      field = &brief.focus;
      continue;
    }
    if (line == "背景" || line == "应用") {
      field = &brief.background;
      continue;
    }
    if (line == "语言") {
      field = &brief.language;
      continue;
    }
    if (line == "机制" || line == "原理") {
      field = &brief.mechanism;
      continue;
    }
    if (line == "程序") {
      field = &brief.program;
      continue;
    }
    if (line == "证明") {
      field = &brief.principle;
      continue;
    }
    if (field != nullptr) {
      *field += line;
      *field += '\n';
    }
  }
  flush_brief();
  flush_group();
  return copy;
}

const Lesson* track_neighbor(const std::vector<Lesson>& lessons, std::size_t index, int direction, const char** label) {
  if (index >= lessons.size() || label == nullptr || (direction != -1 && direction != 1)) {
    return nullptr;
  }
  const Lesson& lesson = lessons[index];
  if (lesson.layer != "方向") {
    if (direction < 0) {
      if (index == 0) {
        return nullptr;
      }
      *label = "上一事实";
      return &lessons[index - 1];
    }
    if (index + 1 >= lessons.size()) {
      return nullptr;
    }
    *label = lessons[index + 1].layer == lesson.layer ? "下一事实" : "进入下一层";
    return &lessons[index + 1];
  }
  if (direction < 0) {
    if (index > 0 && lessons[index - 1].layer == "方向" && lessons[index - 1].group == lesson.group) {
      *label = "上一事实";
      return &lessons[index - 1];
    }
    return nullptr;
  }
  if (index + 1 < lessons.size() && lessons[index + 1].layer == "方向" && lessons[index + 1].group == lesson.group) {
    *label = "下一事实";
    return &lessons[index + 1];
  }
  for (std::size_t cursor = index + 1; cursor < lessons.size(); ++cursor) {
    if (lessons[cursor].layer == "方向" && lessons[cursor].group != lesson.group) {
      *label = "另一方向";
      return &lessons[cursor];
    }
  }
  return nullptr;
}

const Lesson* find_lesson(const std::vector<Lesson>& lessons, const std::string& id) {
  for (const Lesson& lesson : lessons) {
    if (lesson.id == id) {
      return &lesson;
    }
  }
  return nullptr;
}

namespace {

std::vector<std::string> cause_sentences(const std::string& text) {
  std::vector<std::string> sentences;
  const std::string stop = "。";
  std::size_t begin = 0;
  while (begin < text.size()) {
    const std::size_t at = text.find(stop, begin);
    if (at == std::string::npos) {
      const std::string tail = trim_copy(text.substr(begin));
      if (!tail.empty()) {
        sentences.push_back(tail);
      }
      break;
    }
    sentences.push_back(text.substr(begin, at + stop.size() - begin));
    begin = at + stop.size();
  }
  return sentences;
}

bool starts_with_marker(const std::string& sentence) {
  const std::string trimmed = trim_copy(sentence);
  const char* markers[] = {"此后", "随后", "于是", "因此", "所以"};
  for (const char* marker : markers) {
    const std::string prefix = marker;
    if (trimmed.compare(0, prefix.size(), prefix) == 0) {
      return true;
    }
  }
  return false;
}

std::string join_sentences(const std::vector<std::string>& sentences, std::size_t from, std::size_t to) {
  std::string out;
  for (std::size_t index = from; index < to; ++index) {
    out += sentences[index];
  }
  return trim_copy(out);
}

}  // namespace

std::pair<std::string, std::string> split_cause(const std::string& text) {
  const std::size_t blank = text.find("\n\n");
  if (blank != std::string::npos) {
    return {trim_copy(text.substr(0, blank)), trim_copy(text.substr(blank + 2))};
  }
  const std::vector<std::string> sentences = cause_sentences(text);
  if (sentences.size() < 4) {
    return {trim_copy(text), {}};
  }
  for (std::size_t index = 2; index + 1 < sentences.size(); ++index) {
    if (starts_with_marker(sentences[index])) {
      return {join_sentences(sentences, 0, index), join_sentences(sentences, index, sentences.size())};
    }
  }
  return {trim_copy(text), {}};
}

std::string loosen_background(const std::string& text) {
  if (text.find("\n\n") != std::string::npos) {
    return text;
  }
  const std::vector<std::string> sentences = cause_sentences(text);
  if (sentences.size() < 4) {
    return trim_copy(text);
  }
  std::string out;
  for (std::size_t index = 0; index < sentences.size(); ++index) {
    if (index > 0 && index % 2 == 0) {
      out += "\n\n";
    }
    out += trim_copy(sentences[index]);
  }
  return out;
}

std::size_t select_lesson(const std::string& root, const std::string& argument, const std::vector<Lesson>& lessons) {
  if (argument.empty()) {
    throw Error(Error::Code::InvalidArgument, "用法：syslab learn、syslab learn 1、syslab learn next");
  }
  if (argument == "next") {
    const std::string path = join_path(root, "artifacts/learn.next");
    if (!path_exists(path)) {
      return 0;
    }
    const std::string saved = trim_copy(read_file(path));
    if (saved == "end") {
      throw Error(Error::Code::InvalidArgument, "已经是最后一节。从头再看：make learn N=1");
    }
    try {
      const int number = std::stoi(saved);
      if (number < 1 || static_cast<std::size_t>(number) > lessons.size()) {
        return 0;
      }
      return static_cast<std::size_t>(number - 1);
    } catch (const std::exception&) {
      return 0;
    }
  }
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    if (lessons[index].id == argument) {
      return index;
    }
  }
  try {
    const int number = std::stoi(argument);
    if (number < 1 || static_cast<std::size_t>(number) > lessons.size()) {
      throw Error(Error::Code::InvalidArgument, "课次范围是 1 到 " + std::to_string(lessons.size()));
    }
    return static_cast<std::size_t>(number - 1);
  } catch (const Error&) {
    throw;
  } catch (const std::exception&) {
    throw Error(Error::Code::InvalidArgument, "用法：syslab learn、syslab learn 1、syslab learn next");
  }
}

void remember_next(const std::string& root, std::size_t index, std::size_t count, int status) {
  if (status != 0 && status != 3) {
    return;
  }
  const std::string directory = join_path(root, "artifacts");
  create_directories(directory);
  if (index + 1 >= count) {
    write_file(join_path(directory, "learn.next"), "end\n");
    return;
  }
  write_file(join_path(directory, "learn.next"), std::to_string(index + 2) + "\n");
}

}  // namespace syslab
