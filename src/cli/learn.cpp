#include "learn.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/experiment/model.hpp"

#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace syslab {
namespace {

struct Lesson {
  std::string group;
  std::string id;
  std::string title;
  std::string diagnose;
  bool both = false;
  bool disasm = false;
};

struct RunView {
  std::string label;
  std::string run_directory;
  ExperimentState state = ExperimentState::Failed;
};

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
    const std::string paragraph = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
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

std::string display_path(const std::string& root, const std::string& path) {
  const std::string prefix = root.empty() || root.back() == '/' ? root : root + "/";
  if (!prefix.empty() && path.compare(0, prefix.size(), prefix) == 0) {
    return path.substr(prefix.size());
  }
  return path;
}

std::string html_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char character : text) {
    switch (character) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += character;
        break;
    }
  }
  return out;
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

struct LessonBrief {
  std::string application;
  std::string principle;
};

struct LessonCopy {
  std::map<std::string, LessonBrief> briefs;
  std::map<std::string, std::string> groups;
};

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
      brief.application = trim_copy(brief.application);
      brief.principle = trim_copy(brief.principle);
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
    if (line == "应用") {
      field = &brief.application;
      continue;
    }
    if (line == "原理") {
      field = &brief.principle;
      continue;
    }
    if (line == "证明") {
      field = nullptr;
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

std::string basis_label(const std::string& basis) {
  if (basis == "LANG") {
    return "语言规定";
  }
  if (basis == "ABI") {
    return "调用约定";
  }
  if (basis == "COMPILER") {
    return "编译器";
  }
  if (basis == "OS") {
    return "操作系统";
  }
  if (basis == "ISA") {
    return "指令集";
  }
  if (basis == "MICROARCH") {
    return "微架构";
  }
  if (basis == "OBSERVED") {
    return "这次观察到";
  }
  return basis;
}

std::string readme_text(const Manifest& manifest) {
  const std::string path = join_path(manifest.directory, "README.md");
  if (!path_exists(path)) {
    return {};
  }
  return read_file(path);
}

std::vector<std::string> stdout_files(const std::string& directory) {
  std::vector<std::string> files;
  DIR* handle = ::opendir(directory.c_str());
  if (handle == nullptr) {
    return files;
  }
  while (const dirent* entry = ::readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.size() >= 7 && name.compare(name.size() - 7, 7, ".stdout") == 0) {
      files.push_back(join_path(directory, name));
    }
  }
  ::closedir(handle);
  std::sort(files.begin(), files.end());
  return files;
}

std::string latest_run_directory(const std::string& root, const std::string& id) {
  const std::string path = join_path(root, "artifacts/latest/" + id + ".txt");
  if (!path_exists(path)) {
    return {};
  }
  return trim_copy(read_file(path));
}

void append_protocol(std::ostringstream* out, const std::string& line, bool html) {
  auto emit = [&](const std::string& text) {
    if (html) {
      *out << "<p>" << html_escape(text) << "</p>\n";
    } else {
      *out << "  " << text << "\n";
    }
  };
  if (line.compare(0, 12, "SYSLAB_STEP ") == 0) {
    emit(line.substr(12));
    return;
  }
  if (line.compare(0, 14, "SYSLAB_METRIC ") == 0) {
    const std::string body = line.substr(14);
    const std::size_t eq = body.find('=');
    if (eq != std::string::npos) {
      emit(body.substr(0, eq) + " = " + body.substr(eq + 1));
      return;
    }
  }
  if (line.compare(0, 12, "SYSLAB_NOTE ") == 0) {
    const std::string body = line.substr(12);
    const std::size_t text_key = body.find(" text=");
    if (body.compare(0, 6, "basis=") == 0 && text_key != std::string::npos) {
      emit(basis_label(body.substr(6, text_key - 6)) + "：" + body.substr(text_key + 6));
      return;
    }
  }
  if (line.compare(0, 14, "SYSLAB_RESULT ") == 0) {
    const std::string body = line.substr(14);
    std::string status;
    std::string reason;
    std::size_t cursor = 0;
    while (cursor < body.size()) {
      const std::size_t space = body.find(' ', cursor);
      const std::string token = body.substr(cursor, space == std::string::npos ? std::string::npos : space - cursor);
      const std::size_t eq = token.find('=');
      if (eq != std::string::npos) {
        const std::string key = token.substr(0, eq);
        const std::string value = token.substr(eq + 1);
        if (key == "status") {
          status = value;
        } else if (key == "reason") {
          reason = value;
        }
      }
      if (space == std::string::npos) {
        break;
      }
      cursor = space + 1;
    }
    std::string phrase = "结果标记：" + status;
    if (status == "pass") {
      phrase = "结果标记：通过";
    } else if (status == "fail") {
      phrase = "结果标记：未通过";
    } else if (status == "unsupported") {
      phrase = "结果标记：当前主机不支持";
    }
    if (!reason.empty()) {
      phrase += "（" + reason + "）";
    }
    emit(phrase);
    return;
  }
  if (!line.empty()) {
    emit(line);
  }
}

std::string readings_text(const std::string& run_directory, bool html) {
  std::ostringstream out;
  const std::vector<std::string> files = stdout_files(run_directory);
  if (files.empty()) {
    if (html) {
      out << "<p>没有读到程序输出。</p>\n";
    } else {
      out << "  没有读到程序输出。\n";
    }
    return out.str();
  }
  for (std::size_t index = 0; index < files.size(); ++index) {
    if (files.size() > 1) {
      if (html) {
        out << "<p>第 " << (index + 1) << " 次</p>\n";
      } else {
        out << "  第 " << (index + 1) << " 次\n";
      }
    }
    const std::string text = read_file(files[index]);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
      const std::size_t end = text.find('\n', cursor);
      std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      append_protocol(&out, line, html);
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 1;
    }
  }
  return out.str();
}

bool tile_tokens(const std::string& line, std::vector<std::string>* tokens) {
  std::size_t cursor = 0;
  tokens->clear();
  while (cursor < line.size()) {
    while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
      ++cursor;
    }
    if (cursor >= line.size()) {
      break;
    }
    if (line[cursor] != '[') {
      return false;
    }
    const std::size_t close = line.find(']', cursor + 1);
    if (close == std::string::npos) {
      return false;
    }
    tokens->push_back(line.substr(cursor + 1, close - cursor - 1));
    cursor = close + 1;
  }
  return !tokens->empty();
}

bool bits_only(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  for (const char character : token) {
    if (character != '0' && character != '1') {
      return false;
    }
  }
  return true;
}

std::string svg_text(int x, int y, const std::string& text, const char* fill, int size, const char* anchor) {
  std::ostringstream out;
  out << "<text x=\"" << x << "\" y=\"" << y << "\" fill=\"" << fill << "\" font-size=\"" << size
      << "\" font-family=\"Segoe UI, Noto Sans CJK SC, sans-serif\"";
  if (anchor != nullptr && anchor[0] != '\0') {
    out << " text-anchor=\"" << anchor << "\"";
  }
  out << ">" << html_escape(text) << "</text>\n";
  return out.str();
}

std::string figure_html(const std::string& figure) {
  static int diagram_serial = 0;
  ++diagram_serial;
  const std::string marker = "arrow" + std::to_string(diagram_serial);
  const int width = 720;
  int y = 18;
  std::ostringstream body;
  body << "<defs><marker id=\"" << marker << "\" markerWidth=\"8\" markerHeight=\"8\" refX=\"7\" refY=\"3\" orient=\"auto\">"
       << "<path d=\"M0,0 L7,3 L0,6 Z\" fill=\"#1f4e79\"/></marker></defs>\n";
  std::size_t cursor = 0;
  while (cursor < figure.size()) {
    const std::size_t end = figure.find('\n', cursor);
    const std::string raw = figure.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    const std::string line = trim_copy(raw);
    cursor = end == std::string::npos ? figure.size() : end + 1;
    if (line.empty()) {
      continue;
    }
    std::vector<std::string> tokens;
    const std::size_t arrow = line.find("->");
    if (tile_tokens(line, &tokens)) {
      bool bitfield = tokens.size() >= 2;
      int bit_total = 0;
      for (const std::string& token : tokens) {
        if (!bits_only(token)) {
          bitfield = false;
        } else {
          bit_total += static_cast<int>(token.size());
        }
      }
      if (bitfield && bit_total >= 8) {
        const int bar_x = 20;
        const int bar_w = 680;
        const int bar_h = 52;
        int x = bar_x;
        const char* fills[] = {"#1f4e79", "#3d6b94", "#3f6f5b"};
        for (std::size_t index = 0; index < tokens.size(); ++index) {
          int cell_w = std::max(28, bar_w * static_cast<int>(tokens[index].size()) / bit_total);
          if (index + 1 == tokens.size()) {
            cell_w = bar_x + bar_w - x;
          }
          body << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << cell_w << "\" height=\"" << bar_h
               << "\" fill=\"" << fills[index % 3] << "\"/>\n";
          const std::string label = tokens[index].size() <= 10 ? tokens[index] : std::to_string(tokens[index].size()) + " 位";
          body << svg_text(x + cell_w / 2, y + 32, label, "#ffffff", 14, "middle");
          x += cell_w;
        }
        y += bar_h + 22;
      } else {
        int x = 20;
        const int cell_h = 46;
        for (const std::string& token : tokens) {
          const int cell_w = std::max(56, std::min(200, static_cast<int>(token.size()) * 12 + 28));
          if (x + cell_w > width - 16) {
            x = 20;
            y += cell_h + 12;
          }
          body << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << cell_w << "\" height=\"" << cell_h
               << "\" rx=\"6\" fill=\"#1f4e79\"/>\n";
          body << svg_text(x + cell_w / 2, y + 29, token, "#ffffff", 15, "middle");
          x += cell_w + 10;
        }
        y += cell_h + 18;
      }
    } else if (arrow != std::string::npos) {
      const std::string left = trim_copy(line.substr(0, arrow));
      std::size_t right_at = arrow + 2;
      if (right_at < line.size() && line[right_at] == '>') {
        ++right_at;
      }
      const std::string right = trim_copy(line.substr(right_at));
      if (left.empty() || right.empty() || left.size() > 36 || right.size() > 36) {
        body << svg_text(20, y + 16, line, "#1c2430", 15, "");
        y += 28;
        continue;
      }
      body << "<rect x=\"20\" y=\"" << y << "\" width=\"250\" height=\"44\" rx=\"6\" fill=\"#e7eef6\" stroke=\"#1f4e79\"/>\n";
      body << svg_text(145, y + 28, left, "#1c2430", 14, "middle");
      body << "<line x1=\"278\" y1=\"" << (y + 22) << "\" x2=\"430\" y2=\"" << (y + 22)
           << "\" stroke=\"#1f4e79\" stroke-width=\"2\" marker-end=\"url(#" << marker << ")\"/>\n";
      body << "<rect x=\"440\" y=\"" << y << "\" width=\"250\" height=\"44\" rx=\"6\" fill=\"#e7eef6\" stroke=\"#1f4e79\"/>\n";
      body << svg_text(565, y + 28, right, "#1c2430", 14, "middle");
      y += 64;
    } else {
      body << svg_text(20, y + 16, line, "#1c2430", 15, "");
      y += 28;
    }
  }
  y += 10;
  std::ostringstream out;
  out << "<figure class=\"diagram\"><svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << " " << y
      << "\" role=\"img\">\n<rect width=\"" << width << "\" height=\"" << y << "\" fill=\"#f7f9fc\"/>\n" << body.str()
      << "</svg></figure>\n";
  return out.str();
}

std::string paragraph_html(const std::string& text) {
  std::ostringstream out;
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    const std::size_t end = text.find("\n\n", cursor);
    const std::string paragraph = trim_copy(text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor));
    if (!paragraph.empty()) {
      out << "<p>" << html_escape(paragraph) << "</p>\n";
    }
    if (end == std::string::npos) {
      break;
    }
    cursor = end + 2;
  }
  return out.str();
}

std::string disassembly_excerpt(const std::string& run_directory) {
  const std::string path = join_path(run_directory, "disassembly.txt");
  if (!path_exists(path)) {
    return {};
  }
  const std::string text = read_file(path);
  std::ostringstream out;
  int lines = 0;
  std::size_t cursor = 0;
  while (cursor < text.size() && lines < 40) {
    const std::size_t end = text.find('\n', cursor);
    out << text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor) << "\n";
    ++lines;
    if (end == std::string::npos) {
      break;
    }
    cursor = end + 1;
  }
  if (cursor < text.size()) {
    out << "…\n";
  }
  return out.str();
}

const char* kStyle =
    ":root{--bg:#eef1f6;--surface:#fff;--ink:#1c2430;--muted:#5c6778;--line:#d5dde8;--accent:#1f4e79;--accent-soft:#e7eef6}"
    "body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.65 'Segoe UI','Noto Sans CJK SC','Source Han Sans SC',sans-serif}"
    "header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:16px;height:56px;padding:0 20px;"
    "background:var(--surface);border-bottom:1px solid var(--line)}"
    "header a{color:var(--accent);font-weight:650;text-decoration:none}"
    "header label{color:var(--muted);font-size:14px}"
    "header select{font:inherit;color:var(--ink);background:var(--surface);border:1px solid var(--line);border-radius:6px;"
    "padding:6px 10px;min-width:240px;max-width:46vw}"
    ".layout{display:flex;align-items:stretch;min-height:calc(100vh - 56px)}"
    "aside{width:268px;flex:none;position:sticky;top:56px;height:calc(100vh - 56px);overflow:auto;"
    "background:var(--surface);border-right:1px solid var(--line);padding:8px 0 28px}"
    "aside p{margin:18px 16px 4px;color:var(--muted);font-size:12px;font-weight:650;letter-spacing:.04em}"
    "aside a{display:block;margin:1px 8px;padding:7px 10px;border-radius:6px;color:var(--ink);text-decoration:none;font-size:14px}"
    "aside a:hover{background:var(--accent-soft)}"
    "aside a.active{background:var(--accent-soft);color:var(--accent);font-weight:650}"
    "main{flex:1;min-width:0;padding:28px 36px 72px;max-width:920px}"
    "h1{font-size:28px;line-height:1.3;font-weight:650;letter-spacing:-.02em;margin:6px 0 18px}"
    "h2{margin:0 0 14px;font-size:15px;font-weight:650;color:var(--accent)}"
    "h3{margin:14px 0 6px;font-size:13px;font-weight:650;color:var(--muted)}"
    ".meta{margin:0;color:var(--muted);font-size:14px}"
    ".meta a{margin-left:10px}"
    "section{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:20px 22px;margin:0 0 16px}"
    ".diagram{margin:0 0 16px;background:#f7f9fc;border:1px solid var(--line);border-radius:10px;padding:8px}"
    ".diagram svg{width:100%;height:auto;display:block}"
    "pre{margin:0;white-space:pre-wrap;overflow-wrap:anywhere}"
    ".code{max-height:28rem;overflow:auto;background:#15202b;color:#e7edf4;border-radius:8px;padding:14px 16px;"
    "font:13px/1.55 ui-monospace,'Sarasa Mono SC',monospace}"
    "button{appearance:none;background:var(--accent);color:#fff;border:0;border-radius:8px;padding:8px 18px;"
    "font:14px/1.2 inherit;font-weight:650;cursor:pointer}"
    "button:hover{background:#173e61}"
    "button:focus-visible{outline:2px solid #8eb4d6;outline-offset:2px}"
    "ol.proof{margin:8px 0 0;padding-left:1.4em}ol.proof li{margin:8px 0}"
    "form{margin:0 0 16px}"
    "a{color:var(--accent)}"
    "details{margin-top:12px}"
    "summary{cursor:pointer;color:var(--muted)}"
    "article{display:none}"
    "article:target{display:block}"
    "#stage:not(:has(article:target)) article:first-of-type{display:block}"
    "@media(max-width:900px){.layout{display:block}aside{position:static;width:auto;height:auto;max-height:240px;"
    "border-right:0;border-bottom:1px solid var(--line)}main{padding:20px 16px 48px}}";

std::string page_shell(const std::string& title, const std::string& body) {
  std::ostringstream out;
  out << "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
      << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      << "<title>" << html_escape(title) << "</title><style>" << kStyle << "</style></head><body>\n"
      << body << "</body></html>\n";
  return out.str();
}

bool is_source_name(const std::string& name) {
  const char* suffixes[] = {".cpp", ".cc", ".c", ".h", ".hpp", ".S", ".s"};
  for (const char* suffix : suffixes) {
    const std::size_t length = std::strlen(suffix);
    if (name.size() >= length && name.compare(name.size() - length, length, suffix) == 0) {
      return true;
    }
  }
  return false;
}

std::string sources_html(const std::string& directory) {
  std::vector<std::string> names;
  DIR* handle = ::opendir(directory.c_str());
  if (handle != nullptr) {
    while (const dirent* entry = ::readdir(handle)) {
      const std::string name = entry->d_name;
      if (name != "." && name != ".." && is_source_name(name)) {
        names.push_back(name);
      }
    }
    ::closedir(handle);
  }
  std::sort(names.begin(), names.end(), [](const std::string& left, const std::string& right) {
    if (left == "main.cpp") {
      return right != "main.cpp";
    }
    if (right == "main.cpp") {
      return false;
    }
    return left < right;
  });
  if (names.empty()) {
    return "<p>这个目录里没有源码文件。</p>\n";
  }
  std::ostringstream out;
  for (const std::string& name : names) {
    out << "<h3>" << html_escape(name) << "</h3>\n<pre class=\"code\">"
        << html_escape(read_file(join_path(directory, name))) << "</pre>\n";
  }
  return out.str();
}

bool parse_integer(const std::string& text, long long* value) {
  if (text.empty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long long parsed = std::strtoll(text.c_str(), &end, 10);
  if (end == nullptr || *end != '\0' || errno != 0) {
    return false;
  }
  *value = parsed;
  return true;
}

std::string hex_digits(unsigned long long value, int width) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setfill('0') << std::setw(width) << value;
  return out.str();
}

std::string byte_tiles(unsigned long long value) {
  const int width = value > 0xffffffffull ? 16 : 8;
  const std::string digits = hex_digits(value, width);
  const int count = width / 2;
  const int cell = 64;
  const int gap = 8;
  const int svg_w = count * cell + (count - 1) * gap + 8;
  std::ostringstream out;
  out << "<figure class=\"diagram\"><svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << svg_w
      << " 72\" role=\"img\">\n<rect width=\"" << svg_w << "\" height=\"72\" fill=\"#f7f9fc\"/>\n";
  for (int index = 0; index < count; ++index) {
    const int x = 4 + index * (cell + gap);
    out << "<rect x=\"" << x << "\" y=\"8\" width=\"" << cell << "\" height=\"48\" rx=\"6\" fill=\"#1f4e79\"/>\n";
    out << svg_text(x + cell / 2, 38, digits.substr(static_cast<std::size_t>(index) * 2, 2), "#ffffff", 16, "middle");
  }
  out << "</svg></figure>\n<p>十进制 " << value << "，十六进制 " << digits << "。</p>\n";
  return out.str();
}

std::string result_sentence(const std::string& status, const std::string& reason, bool proved) {
  std::string phrase = "进程已结束。";
  if (status == "pass") {
    phrase = proved ? "以上各步的观测与图中的结构一致。" : "结果标记为通过。这次输出没有逐步观测。";
  } else if (status == "fail") {
    phrase = proved ? "证明停在未成立的一步。" : "结果标记为未通过。";
  } else if (status == "unsupported") {
    phrase = "当前主机不能完成这一项观察。";
  }
  if (!reason.empty()) {
    phrase += "标记：" + reason + "。";
  }
  return phrase;
}

void read_program_lines(const std::string& run_directory, std::vector<std::string>* steps,
                        std::vector<std::pair<std::string, std::string>>* metrics, std::string* status,
                        std::string* reason) {
  for (const std::string& file : stdout_files(run_directory)) {
    const std::string text = read_file(file);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
      const std::size_t end = text.find('\n', cursor);
      std::string line = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line.compare(0, 12, "SYSLAB_STEP ") == 0) {
        steps->push_back(line.substr(12));
      } else if (line.compare(0, 14, "SYSLAB_METRIC ") == 0) {
        const std::string body = line.substr(14);
        const std::size_t eq = body.find('=');
        if (eq != std::string::npos) {
          metrics->emplace_back(body.substr(0, eq), body.substr(eq + 1));
        }
      } else if (line.compare(0, 14, "SYSLAB_RESULT ") == 0) {
        const std::string body = line.substr(14);
        std::size_t token_at = 0;
        while (token_at < body.size()) {
          const std::size_t space = body.find(' ', token_at);
          const std::string token = body.substr(token_at, space == std::string::npos ? std::string::npos : space - token_at);
          const std::size_t eq = token.find('=');
          if (eq != std::string::npos) {
            if (token.substr(0, eq) == "status") {
              *status = token.substr(eq + 1);
            } else if (token.substr(0, eq) == "reason") {
              *reason = token.substr(eq + 1);
            }
          }
          if (space == std::string::npos) {
            break;
          }
          token_at = space + 1;
        }
      }
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 1;
    }
  }
}

std::string demo_html(const std::string& lesson_id, const std::string& checks, const std::string& figure,
                      const std::vector<RunView>& runs, const std::string& latest) {
  std::ostringstream out;
  out << "<form method=\"post\" action=\"/run/" << html_escape(lesson_id) << "\"><button type=\"submit\">执行</button></form>\n";
  (void)figure;
  if (!checks.empty()) {
    out << "<p>验证条件：" << html_escape(checks) << "</p>\n";
  }
  const std::vector<RunView>* shown = &runs;
  std::vector<RunView> from_latest;
  if (runs.empty()) {
    if (latest.empty()) {
      out << "<p>本节尚未在当前主机执行。执行后，程序按图中的顺序打印每一步观测。</p>\n";
      return out.str();
    }
    from_latest.push_back(RunView{"", latest, ExperimentState::Passed});
    shown = &from_latest;
  }
  for (const RunView& run : *shown) {
    if (run.run_directory.empty()) {
      continue;
    }
    if (!run.label.empty()) {
      out << "<p><strong>" << html_escape(run.label) << "</strong></p>\n";
    }
    std::vector<std::string> steps;
    std::vector<std::pair<std::string, std::string>> metrics;
    std::string status;
    std::string reason;
    read_program_lines(run.run_directory, &steps, &metrics, &status, &reason);
    if (steps.empty() && metrics.empty() && status.empty()) {
      out << "<p>标准输出中没有逐步观测。</p>\n";
    }
    if (!steps.empty()) {
      out << "<ol class=\"proof\">\n";
      for (const std::string& step : steps) {
        out << "<li>" << html_escape(step) << "</li>\n";
      }
      out << "</ol>\n";
    }
    for (const auto& metric : metrics) {
      long long number = 0;
      if (parse_integer(metric.second, &number) && number >= 256) {
        out << "<p>" << html_escape(metric.first) << " 的内存字节</p>\n";
        out << byte_tiles(static_cast<unsigned long long>(number));
      } else {
        out << "<p>读数 " << html_escape(metric.first + " = " + metric.second) << "</p>\n";
      }
    }
    if (!status.empty()) {
      out << "<p><strong>" << html_escape(result_sentence(status, reason, !steps.empty())) << "</strong></p>\n";
    } else if (!runs.empty()) {
      out << "<p><strong>" << state_phrase(run.state) << "</strong></p>\n";
    }
    const std::string excerpt = disassembly_excerpt(run.run_directory);
    if (!excerpt.empty()) {
      out << "<details><summary>反汇编前 40 行</summary><pre>" << html_escape(excerpt) << "</pre></details>\n";
    }
  }
  return out.str();
}

void remove_split_pages(const std::string& directory) {
  DIR* handle = ::opendir(directory.c_str());
  if (handle == nullptr) {
    return;
  }
  while (const dirent* entry = ::readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".html") == 0 && name != "index.html") {
      std::remove(join_path(directory, name).c_str());
    }
  }
  ::closedir(handle);
}

void write_course_page(const std::string& root, const std::vector<Lesson>& lessons, const Registry& registry,
                       const std::map<std::string, std::string>& figures, const LessonCopy& copy,
                       const std::string& fresh_id, const std::vector<RunView>& fresh_runs) {
  const std::string directory = join_path(root, "artifacts/lessons");
  create_directories(directory);
  remove_split_pages(directory);
  std::ostringstream page;
  page << "<header><a href=\"#" << html_escape(lessons.front().id) << "\">目录</a>"
       << "<label>跳到 <select id=\"jump\">\n";
  std::string group;
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    const Lesson& lesson = lessons[index];
    if (lesson.group != group) {
      if (!group.empty()) {
        page << "</optgroup>\n";
      }
      group = lesson.group;
      page << "<optgroup label=\"" << html_escape(group) << "\">\n";
    }
    page << "<option value=\"" << html_escape(lesson.id) << "\">" << (index + 1) << " " << html_escape(lesson.title)
         << "</option>\n";
  }
  if (!group.empty()) {
    page << "</optgroup>\n";
  }
  page << "</select></label></header>\n<div class=\"layout\">\n<aside>\n";
  group.clear();
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    const Lesson& lesson = lessons[index];
    if (lesson.group != group) {
      group = lesson.group;
      page << "<p>" << html_escape(group) << "</p>\n";
    }
    page << "<a href=\"#" << html_escape(lesson.id) << "\">" << (index + 1) << " " << html_escape(lesson.title)
         << "</a>\n";
  }
  page << "</aside>\n<main id=\"stage\">\n";
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    const Lesson& lesson = lessons[index];
    const Manifest& manifest = registry.get(lesson.id);
    const std::string readme = readme_text(manifest);
    const std::string checks = markdown_section(readme, "程序实际检查的内容");
    const std::string limit = without_runner_notice(markdown_section(readme, "不由本实验断言的内容"));
    const auto figure = figures.find(lesson.id);
    const bool fresh = lesson.id == fresh_id && !fresh_runs.empty();
    page << "<article id=\"" << html_escape(lesson.id) << "\">\n";
    page << "<p class=\"meta\">第 " << (index + 1) << " 节 / " << lessons.size() << " · " << html_escape(lesson.group);
    if (index > 0) {
      page << " · <a href=\"#" << html_escape(lessons[index - 1].id) << "\">上一节</a>";
    }
    if (index + 1 < lessons.size()) {
      page << " · <a href=\"#" << html_escape(lessons[index + 1].id) << "\">下一节</a>";
    }
    page << "</p>\n<h1>" << html_escape(lesson.title) << "</h1>\n";
    const auto brief = copy.briefs.find(lesson.id);
    const auto group_text = copy.groups.find(lesson.group);
    page << "<section><h2>应用中的问题</h2>\n";
    if (group_text != copy.groups.end()) {
      page << paragraph_html(group_text->second);
    }
    if (brief != copy.briefs.end() && !brief->second.application.empty()) {
      page << paragraph_html(brief->second.application);
    }
    page << "</section>\n<section><h2>这一层的机制</h2>\n";
    if (brief != copy.briefs.end() && !brief->second.principle.empty()) {
      page << paragraph_html(brief->second.principle);
    }
    const std::string figure_text = figure == figures.end() ? std::string() : figure->second;
    if (!figure_text.empty()) {
      page << figure_html(figure_text);
    }
    if (!checks.empty()) {
      page << "<p>程序要成立的事实：" << html_escape(checks) << "</p>\n";
    }
    page << "</section>\n<section><h2>程序证明</h2>\n"
         << demo_html(lesson.id, "", "", fresh ? fresh_runs : std::vector<RunView>{},
                      fresh ? std::string() : latest_run_directory(root, lesson.id))
         << "</section>\n<section><h2>证明到哪一层</h2>\n";
    if (!limit.empty()) {
      page << paragraph_html(limit);
    }
    page << "<p>出现同类故障时，先核对：" << html_escape(lesson.diagnose) << "</p>\n";
    page << "</section>\n<section><h2>源码</h2>\n" << sources_html(manifest.directory) << "</section>\n</article>\n";
  }
  page << "</main></div>\n<script>\n"
          "function show(id){var arts=document.querySelectorAll('article');var found=false;"
          "for(var i=0;i<arts.length;i++){var on=arts[i].id===id;arts[i].style.display=on?'block':'none';if(on)found=true;}"
          "if(!found&&arts.length){arts[0].style.display='block';id=arts[0].id;}"
          "var links=document.querySelectorAll('aside a');"
          "for(var j=0;j<links.length;j++){links[j].className=links[j].getAttribute('href')==='#'+id?'active':'';}"
          "var jump=document.getElementById('jump');if(jump)jump.value=id;"
          "if(location.hash!=='#'+id)history.replaceState(null,'','#'+id);}\n"
          "document.getElementById('jump').addEventListener('change',function(){show(this.value);});\n"
          "window.addEventListener('hashchange',function(){show(location.hash.slice(1));});\n"
          "show(location.hash.slice(1));\n"
          "</script>\n";
  write_file(join_path(directory, "index.html"), page_shell("目录", page.str()));
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
  std::cout << "页面    artifacts/lessons/index.html\n\n";
  std::string group;
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    const Lesson& lesson = lessons[index];
    if (lesson.group != group) {
      group = lesson.group;
      std::cout << group << "\n";
    }
    std::cout << "  " << std::setw(2) << (index + 1) << "  " << lesson.title << "\n";
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
  std::cout << lesson.group << "\n\n";
  const auto group_text = copy.groups.find(lesson.group);
  if (group_text != copy.groups.end()) {
    std::cout << group_text->second << "\n\n";
  }
  const auto brief = copy.briefs.find(lesson.id);
  if (brief != copy.briefs.end()) {
    std::cout << "应用中的问题\n" << brief->second.application << "\n\n";
    std::cout << "这一层的机制\n" << brief->second.principle << "\n\n";
  }
  const auto figure = figures.find(lesson.id);
  if (figure != figures.end() && !figure->second.empty()) {
    std::cout << figure->second << "\n\n";
  }
  std::cout << "程序要成立的事实\n" << (checks.empty() ? lesson.diagnose : checks) << "\n\n";
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

}  // namespace

int command_learn(const std::string& root, const std::string& argument, const SessionConfig& config, Registry& registry) {
  const std::vector<Lesson> lessons = load_course(root, registry);
  const std::map<std::string, std::string> figures = load_figures(root);
  const LessonCopy copy = load_briefs(root);
  if (argument.empty()) {
    write_course_page(root, lessons, registry, figures, copy, "", {});
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
  write_course_page(root, lessons, registry, figures, copy, lesson.id, runs);
  print_terminal(root, lessons, index, registry, figures, copy, runs);
  std::cout << "结果：" << phrase_for_code(status) << "\n";
  remember_next(root, index, lessons.size(), status);
  return status;
}

namespace {

bool send_all(int fd, const char* data, std::size_t size) {
  std::size_t sent = 0;
  while (sent < size) {
    const ssize_t count = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

std::string request_path(const std::string& request) {
  if (request.compare(0, 4, "GET ") != 0 && request.compare(0, 5, "HEAD ") != 0 && request.compare(0, 5, "POST ") != 0) {
    return {};
  }
  const std::size_t start = request.find(' ');
  if (start == std::string::npos) {
    return {};
  }
  const std::size_t path_begin = start + 1;
  const std::size_t path_end = request.find(' ', path_begin);
  if (path_end == std::string::npos) {
    return {};
  }
  std::string path = request.substr(path_begin, path_end - path_begin);
  const std::size_t query = path.find('?');
  if (query != std::string::npos) {
    path.resize(query);
  }
  return path;
}

void reply_redirect(int client, const std::string& location) {
  const std::string header = "HTTP/1.1 303 See Other\r\nLocation: " + location + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  send_all(client, header.data(), header.size());
}

void reply_html(int client, int status, const std::string& body, bool head_only) {
  const char* phrase = "OK";
  if (status == 404) {
    phrase = "Not Found";
  } else if (status != 200) {
    phrase = "Error";
  }
  std::ostringstream header;
  header << "HTTP/1.1 " << status << " " << phrase << "\r\n"
         << "Content-Type: text/html; charset=utf-8\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n\r\n";
  const std::string head = header.str();
  if (!send_all(client, head.data(), head.size())) {
    return;
  }
  if (!head_only && !body.empty()) {
    send_all(client, body.data(), body.size());
  }
}

}  // namespace

int command_web(const std::string& root, int port, const SessionConfig& config, Registry& registry) {
  if (port < 1 || port > 65535) {
    throw Error(Error::Code::InvalidArgument, "端口应为 1 到 65535 的整数");
  }
  const std::vector<Lesson> lessons = load_course(root, registry);
  const std::map<std::string, std::string> figures = load_figures(root);
  const LessonCopy copy = load_briefs(root);
  write_course_page(root, lessons, registry, figures, copy, "", {});
  const std::string page_path = join_path(root, "artifacts/lessons/index.html");

  const int server = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    throw Error(Error::Code::Io, "无法创建网页套接字");
  }
  const int reuse = 1;
  ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(server);
    throw Error(Error::Code::Io, "端口 " + std::to_string(port) + " 无法绑定。换一个：make web PORT=9528");
  }
  if (::listen(server, 16) != 0) {
    ::close(server);
    throw Error(Error::Code::Io, "网页服务无法开始监听");
  }
  std::cout << "http://127.0.0.1:" << port << "/\n";
  std::cout << "页面中的「执行」在当前主机编译并运行该节。Ctrl+C 停止服务。\n";
  std::cout.flush();

  while (true) {
    const int client = ::accept(server, nullptr, nullptr);
    if (client < 0) {
      if (errno == EINTR) {
        continue;
      }
      ::close(server);
      throw Error(Error::Code::Io, "接受网页连接失败");
    }
    std::string request;
    char buffer[1024];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
      const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
      if (count < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if (count == 0) {
        break;
      }
      request.append(buffer, static_cast<std::size_t>(count));
    }
    const bool head_only = request.compare(0, 5, "HEAD ") == 0;
    const bool post = request.compare(0, 5, "POST ") == 0;
    const std::string path = request_path(request);
    if (post && path.compare(0, 5, "/run/") == 0 && path.find('/', 5) == std::string::npos) {
      const std::string id = path.substr(5);
      std::size_t index = lessons.size();
      for (std::size_t cursor = 0; cursor < lessons.size(); ++cursor) {
        if (lessons[cursor].id == id) {
          index = cursor;
          break;
        }
      }
      if (index == lessons.size()) {
        reply_html(client, 404, "<!DOCTYPE html><meta charset=\"utf-8\"><p>没有这一节。</p>\n", false);
      } else {
        try {
          const Lesson& lesson = lessons[index];
          const Manifest& manifest = registry.get(lesson.id);
          std::vector<RunView> runs;
          SessionConfig run_config = config;
          run_config.variant.clear();
          const SessionOutput first = execute_session(manifest, run_config);
          runs.push_back(RunView{lesson.both ? "未优化" : "", first.run_directory, first.state});
          if (lesson.both) {
            run_config.variant = "optimized";
            const SessionOutput second = execute_session(manifest, run_config);
            runs.push_back(RunView{"优化后", second.run_directory, second.state});
          }
          write_course_page(root, lessons, registry, figures, copy, lesson.id, runs);
          reply_redirect(client, "/#" + lesson.id);
        } catch (const std::exception& error) {
          reply_html(client, 500,
                     "<!DOCTYPE html><meta charset=\"utf-8\"><p>运行没有完成：" + html_escape(error.what()) + "</p>\n",
                     false);
        }
      }
    } else if (path == "/" || path == "/index.html") {
      reply_html(client, 200, read_file(page_path), head_only);
    } else {
      reply_html(client, 404, "<!DOCTYPE html><meta charset=\"utf-8\"><p>这个地址没有页面。</p>\n", head_only);
    }
    ::close(client);
  }
  ::close(server);
  return 0;
}

}  // namespace syslab
