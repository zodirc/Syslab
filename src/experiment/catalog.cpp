#include "syslab/experiment/catalog.hpp"

#include "syslab/core/fs.hpp"

#include <map>
#include <set>

namespace syslab {
namespace {

std::string trim_copy(std::string text) {
  std::size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\n' || text[begin] == '\r' || text[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\n' || text[end - 1] == '\r' || text[end - 1] == '\t')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::vector<std::string> split_cells(const std::string& line) {
  std::vector<std::string> cells;
  std::size_t cursor = 0;
  while (cursor <= line.size()) {
    const std::size_t at = line.find('|', cursor);
    if (at == std::string::npos) {
      cells.push_back(trim_copy(line.substr(cursor)));
      break;
    }
    cells.push_back(trim_copy(line.substr(cursor, at - cursor)));
    cursor = at + 1;
  }
  return cells;
}

std::string strip_code(std::string text) {
  text = trim_copy(std::move(text));
  if (text.size() >= 2 && text.front() == '`' && text.back() == '`') {
    text = text.substr(1, text.size() - 2);
  }
  return trim_copy(text);
}

bool directory_matches(const std::string& actual, const std::string& root, const std::string& relative) {
  const std::string joined = join_path(root, relative);
  if (actual == joined || actual == relative) {
    return true;
  }
  const std::string suffix = std::string("/") + relative;
  return actual.size() >= suffix.size() && actual.compare(actual.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void read_index(const std::string& text, std::map<std::string, std::string>* directories,
                std::vector<std::string>* issues) {
  std::size_t cursor = 0;
  int line_number = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    cursor = end == std::string::npos ? text.size() : end + 1;
    ++line_number;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] != '|') {
      continue;
    }
    const std::vector<std::string> cells = split_cells(line);
    if (cells.size() < 3) {
      continue;
    }
    const std::string identifier = cells[1];
    if (identifier.empty() || identifier == "ID" || identifier.find("---") != std::string::npos) {
      continue;
    }
    const std::string directory = strip_code(cells[2]);
    if (directories->find(identifier) != directories->end()) {
      issues->push_back("docs/experiments/index.md 第 " + std::to_string(line_number) + " 行重复标识 " + identifier);
      continue;
    }
    if (directory.empty()) {
      issues->push_back("docs/experiments/index.md 第 " + std::to_string(line_number) + " 行没有目录");
      continue;
    }
    directories->emplace(identifier, directory);
  }
}

void read_course(const std::string& text, const Registry& registry, std::vector<std::string>* issues) {
  std::set<std::string> seen;
  std::size_t cursor = 0;
  int line_number = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    cursor = end == std::string::npos ? text.size() : end + 1;
    ++line_number;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const std::string trimmed = trim_copy(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }
    const std::size_t bar = trimmed.find('|');
    const std::string identifier = trim_copy(bar == std::string::npos ? trimmed : trimmed.substr(0, bar));
    if (identifier.empty()) {
      issues->push_back("docs/learn/course.txt 第 " + std::to_string(line_number) + " 行没有实验标识");
      continue;
    }
    if (!seen.insert(identifier).second) {
      issues->push_back("docs/learn/course.txt 重复标识 " + identifier);
    }
    if (!registry.contains(identifier)) {
      issues->push_back("docs/learn/course.txt 中的 " + identifier + " 未在注册表中");
    }
  }
}

}  // namespace

std::vector<std::string> check_catalog(const std::string& source_root, const Registry& registry) {
  std::vector<std::string> issues;
  const char* required_files[] = {"experiment.yaml", "main.cpp", "README.md", "expected/behavior.md"};
  for (const std::string& identifier : registry.ids()) {
    const Manifest& manifest = registry.get(identifier);
    for (const char* relative : required_files) {
      const std::string path = join_path(manifest.directory, relative);
      if (!path_exists(path)) {
        issues.push_back(identifier + " 缺少 " + relative);
      }
    }
  }

  const std::string index_path = join_path(source_root, "docs/experiments/index.md");
  std::map<std::string, std::string> indexed;
  if (!path_exists(index_path)) {
    issues.push_back("缺少 docs/experiments/index.md");
  } else {
    read_index(read_file(index_path), &indexed, &issues);
    for (const auto& entry : indexed) {
      if (!registry.contains(entry.first)) {
        issues.push_back("索引中的 " + entry.first + " 未在注册表中");
        continue;
      }
      const Manifest& manifest = registry.get(entry.first);
      if (!directory_matches(manifest.directory, source_root, entry.second)) {
        issues.push_back(entry.first + " 的索引目录 " + entry.second + " 与清单目录 " + manifest.directory +
                         " 不一致");
      }
    }
    for (const std::string& identifier : registry.ids()) {
      if (indexed.find(identifier) == indexed.end()) {
        issues.push_back("注册表中的 " + identifier + " 不在 docs/experiments/index.md");
      }
    }
  }

  const std::string course_path = join_path(source_root, "docs/learn/course.txt");
  if (!path_exists(course_path)) {
    issues.push_back("缺少 docs/learn/course.txt");
  } else {
    read_course(read_file(course_path), registry, &issues);
  }
  return issues;
}

}  // namespace syslab
