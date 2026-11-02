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

void emit_line(std::ostringstream* out, const std::string& text, bool html) {
  if (html) {
    *out << "<p>" << html_escape(text) << "</p>\n";
  } else {
    *out << "  " << text << "\n";
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
    const ProtocolRecord protocol = parse_protocol(text);
    for (const std::string& step : protocol.steps) {
      emit_line(&out, step, html);
    }
    for (const ProtocolMetric& metric : protocol.metrics) {
      emit_line(&out, metric.name + " = " + metric.value, html);
    }
    for (const ProtocolNote& note : protocol.notes) {
      emit_line(&out, basis_label(note.basis) + "：" + note.text, html);
    }
    if (!protocol.status.empty()) {
      std::string phrase = "结果标记：" + protocol.status;
      if (protocol.status == "pass") {
        phrase = "结果标记：通过";
      } else if (protocol.status == "fail") {
        phrase = "结果标记：未通过";
      } else if (protocol.status == "unsupported") {
        phrase = "结果标记：当前主机不支持";
      }
      if (!protocol.reason.empty()) {
        phrase += "（" + protocol.reason + "）";
      }
      emit_line(&out, phrase, html);
    }
    for (const ProtocolDiagnostic& error : protocol.errors) {
      emit_line(&out, "格式错误：" + error.message, html);
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

std::string figure_html(const std::string& figure, const std::string& caption) {
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
      auto columns = [](const std::string& text) {
        int count = 0;
        for (unsigned char byte : text) {
          if ((byte & 0xc0) != 0x80) {
            ++count;
          }
        }
        return count;
      };
      const int width_columns = std::max(columns(left), columns(right));
      if (left.empty() || right.empty() || width_columns > 18) {
        body << svg_text(20, y + 16, line, "#1c2430", 15, "");
        y += 28;
        continue;
      }
      const int font = width_columns > 12 ? 12 : 14;
      const int text_y = y + 22 + font / 2;
      body << "<rect x=\"20\" y=\"" << y << "\" width=\"250\" height=\"44\" rx=\"6\" fill=\"#e7eef6\" stroke=\"#1f4e79\"/>\n";
      body << svg_text(145, text_y, left, "#1c2430", font, "middle");
      body << "<line x1=\"278\" y1=\"" << (y + 22) << "\" x2=\"430\" y2=\"" << (y + 22)
           << "\" stroke=\"#1f4e79\" stroke-width=\"2\" marker-end=\"url(#" << marker << ")\"/>\n";
      body << "<rect x=\"440\" y=\"" << y << "\" width=\"250\" height=\"44\" rx=\"6\" fill=\"#e7eef6\" stroke=\"#1f4e79\"/>\n";
      body << svg_text(565, text_y, right, "#1c2430", font, "middle");
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
      << "</svg>\n";
  if (!caption.empty()) {
    out << "<figcaption>" << html_escape(caption) << "</figcaption>\n";
  }
  out << "</figure>\n";
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
    "header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:16px;min-height:56px;padding:8px 20px;flex-wrap:wrap;"
    "background:var(--surface);border-bottom:1px solid var(--line)}"
    "header a{color:var(--accent);font-weight:650;text-decoration:none}"
    "header label{color:var(--muted);font-size:14px}"
    "header select{font:inherit;color:var(--ink);background:var(--surface);border:1px solid var(--line);border-radius:6px;"
    "padding:6px 10px;min-width:240px;max-width:46vw}"
    ".layout{display:flex;align-items:stretch;min-height:calc(100vh - 56px)}"
    "aside{width:268px;flex:none;position:sticky;top:56px;height:calc(100vh - 56px);overflow:auto;"
    "background:var(--surface);border-right:1px solid var(--line);padding:8px 0 28px}"
    "aside details{margin:0 0 4px}"
    "aside summary{cursor:pointer;padding:8px 16px;font-weight:650}"
    "aside p{margin:18px 16px 4px;color:var(--muted);font-size:12px;font-weight:650;letter-spacing:.04em}"
    "aside a{display:block;margin:1px 8px;padding:7px 10px;border-radius:6px;color:var(--ink);text-decoration:none;font-size:14px}"
    "aside a:hover{background:var(--accent-soft)}"
    "aside a.active{background:var(--accent-soft);color:var(--accent);font-weight:650}"
    "main{flex:1;min-width:0;padding:28px 36px 72px;max-width:40rem}"
    "nav.path{display:flex;gap:8px;margin:0}"
    "article{scroll-margin-top:96px}"
    "ol.chain{display:flex;gap:8px;list-style:none;padding:0;margin:0 0 16px}"
    "ol.chain li{flex:1;min-width:0;background:var(--surface);border:1px solid var(--line);border-radius:10px;padding:10px 12px}"
    "ol.chain li.now{border-color:var(--accent);background:var(--accent-soft)}"
    "ol.chain span{display:block;color:var(--muted);font-size:12px}"
    "ol.chain strong,ol.chain a{display:block;font-weight:650}"
    "ol.chain p{margin:6px 0 0;font-size:14px}"
    "ol.chain p.where{color:var(--muted);font-size:12px}"
    "nav.path a{padding:4px 12px;border-radius:999px;background:var(--accent-soft);text-decoration:none;font-size:14px;font-weight:650}"
    "nav.path a.on{background:var(--accent);color:#fff}"
    "h1{font-size:28px;line-height:1.35;font-weight:650;letter-spacing:-.02em;margin:4px 0 8px}"
    "p.ask{margin:0 0 18px;font-size:18px;line-height:1.5;font-weight:650}"
    "section.lead{border-left:4px solid var(--accent)}"
    "section.lead p,section p{margin:0 0 0.9em;line-height:1.8}"
    "section.lead p:last-child,section p:last-child{margin-bottom:0}"
    "p.term{margin:-4px 0 16px;color:var(--muted);line-height:1.6}"
    ".layer{margin:0 0 14px;padding:0 0 0 12px;border-left:3px solid var(--accent)}"
    ".layer h3{margin:0 0 4px;color:var(--accent);font-size:13px}"
    "figcaption{margin:8px 4px 0;color:var(--muted);font-size:14px}"
    "h2{margin:0 0 14px;font-size:15px;font-weight:650;color:var(--accent)}"
    "h3{margin:14px 0 6px;font-size:13px;font-weight:650;color:var(--muted)}"
    ".meta{margin:0;color:var(--muted);font-size:14px}"
    ".meta a{margin-left:10px}"
    "section{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:20px 22px;margin:0 0 16px}"
    ".diagram{margin:0 0 16px;background:#f7f9fc;border:1px solid var(--line);border-radius:10px;padding:8px}"
    ".diagram svg,.diagram img{width:100%;height:auto;display:block}"
    ".lab{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:16px 18px;margin:0 0 16px}"
    ".lab label{display:inline-block;margin:0 16px 8px 0;color:var(--muted);font-size:14px}"
    ".lab input{font:inherit;width:8em;margin-left:6px;padding:4px 8px;border:1px solid var(--line);border-radius:6px;color:var(--ink)}"
    ".lab p{margin:8px 0 0}"
    "button.plain{background:var(--surface);color:var(--accent);border:1px solid var(--accent)}"
    "label.expect-label{display:block;color:var(--muted);font-size:14px;margin:0 0 12px}"
    "textarea.expect{display:block;width:100%;box-sizing:border-box;min-height:4.8em;margin-top:6px;font:inherit;color:var(--ink);border:1px solid var(--line);border-radius:8px;padding:8px}"
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
    "border-right:0;border-bottom:1px solid var(--line)}main{padding:20px 16px 48px}ol.chain{flex-direction:column}}";

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
    const ProtocolRecord protocol = parse_protocol(read_file(file));
    steps->insert(steps->end(), protocol.steps.begin(), protocol.steps.end());
    for (const ProtocolMetric& metric : protocol.metrics) {
      metrics->emplace_back(metric.name, metric.value);
    }
    if (!protocol.status.empty()) {
      *status = protocol.status;
      *reason = protocol.reason;
    }
  }
}

std::string expect_html(const std::string& lesson_id, const std::string& prompt) {
  std::ostringstream out;
  const std::string hint = prompt.empty() ? "按上面的规则写下本机应出现的读数" : prompt;
  out << "<section class=\"expect-block\"><h2>先写下预期</h2>\n"
         "<label class=\"expect-label\">按上面的规则写下本机应出现的读数<textarea class=\"expect\" id=\"expect-"
      << html_escape(lesson_id) << "\" data-lesson=\"" << html_escape(lesson_id) << "\" placeholder=\""
      << html_escape(hint) << "\"></textarea></label>\n</section>\n";
  return out.str();
}

std::string demo_html(const std::string& lesson_id, const std::string& checks, const std::string& figure,
                      const std::vector<RunView>& runs, const std::string& latest, const std::string& run_token) {
  std::ostringstream out;
  out << "<p id=\"expect-note-" << html_escape(lesson_id) << "\"></p>\n";
  out << "<form method=\"post\" action=\"/run/" << html_escape(lesson_id) << "\" data-lesson=\"" << html_escape(lesson_id)
      << "\">";
  if (!run_token.empty()) {
    out << "<input type=\"hidden\" name=\"token\" value=\"" << html_escape(run_token) << "\">";
  }
  out << "<button type=\"submit\">执行</button> <button type=\"button\" class=\"plain\" data-reveal=\""
      << html_escape(lesson_id) << "\">对照观测</button></form>\n";
  (void)figure;
  if (!checks.empty()) {
    out << "<p>验证条件：" << html_escape(checks) << "</p>\n";
  }
  out << "<div id=\"obs-" << html_escape(lesson_id) << "\" hidden>\n";
  const std::vector<RunView>* shown = &runs;
  std::vector<RunView> from_latest;
  if (runs.empty()) {
    if (latest.empty()) {
      out << "<p>本节尚未在当前主机执行。按下执行后，观测按程序打印的顺序出现在这里。</p>\n</div>\n";
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
  out << "</div>\n";
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

std::string base64_encode(const std::string& input) {
  static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.size() + 2) / 3) * 4);
  std::size_t index = 0;
  while (index + 3 <= input.size()) {
    const unsigned value = (static_cast<unsigned char>(input[index]) << 16) |
                           (static_cast<unsigned char>(input[index + 1]) << 8) |
                           static_cast<unsigned char>(input[index + 2]);
    out.push_back(kTable[(value >> 18) & 63]);
    out.push_back(kTable[(value >> 12) & 63]);
    out.push_back(kTable[(value >> 6) & 63]);
    out.push_back(kTable[value & 63]);
    index += 3;
  }
  if (index < input.size()) {
    unsigned value = static_cast<unsigned char>(input[index]) << 16;
    out.push_back(kTable[(value >> 18) & 63]);
    if (index + 1 < input.size()) {
      value |= static_cast<unsigned char>(input[index + 1]) << 8;
      out.push_back(kTable[(value >> 12) & 63]);
      out.push_back(kTable[(value >> 6) & 63]);
    } else {
      out.push_back(kTable[(value >> 12) & 63]);
      out.push_back('=');
    }
    out.push_back('=');
  }
  return out;
}

std::string motion_caption(const std::string& id) {
  if (id == "OBJ-001") {
    return "在 C++ 对象模型中，指针成员得到地址。源存储从 7 改为 9 后，经该地址读到 9。另行分配的存储仍为 7。";
  }
  if (id == "SRV-002") {
    return "字节逐次进入缓冲。长度或载荷未齐时请求数保持 0，ping 收齐后请求数变为 1。";
  }
  if (id == "STO-004") {
    return "相反顺序时两把锁互相等待，更新数停在 0。相同顺序时两把锁依次获得，更新数变为 2。";
  }
  return {};
}

std::string motion_html(const std::string& root, const std::string& id, const std::string& caption) {
  const std::string path = join_path(join_path(root, "docs/learn/media"), id + ".gif");
  if (!path_exists(path)) {
    return {};
  }
  const std::string bytes = read_file(path);
  if (bytes.size() < 6 || bytes.compare(0, 3, "GIF") != 0) {
    return {};
  }
  std::ostringstream out;
  out << "<figure class=\"diagram\"><img alt=\"" << html_escape(caption) << "\" src=\"data:image/gif;base64,"
      << base64_encode(bytes) << "\">\n<figcaption>" << html_escape(caption) << "</figcaption></figure>\n";
  return out.str();
}

std::string interactive_html(const std::string& id) {
  if (id == "OBJ-001") {
    return "<section class=\"lab\"><h2>填数观察</h2>\n"
           "<p>按上面的原理填写两个读数，再按对照。本机执行使用 7 和 9。</p>\n"
           "<label>复制时的源值 <input id=\"obj-copy-at\" type=\"number\" value=\"7\"></label>\n"
           "<label>随后写入源 <input id=\"obj-copy-to\" type=\"number\" value=\"9\"></label>\n"
           "<label>浅拷贝应读到 <input id=\"obj-guess-shallow\" type=\"number\"></label>\n"
           "<label>深拷贝应读到 <input id=\"obj-guess-deep\" type=\"number\"></label>\n"
           "<p><button type=\"button\" class=\"plain\" id=\"obj-check\">对照</button></p>\n"
           "<p id=\"obj-result\" hidden></p>\n"
           "</section>\n";
  }
  if (id == "CPU-001") {
    return "<section class=\"lab\"><h2>填数观察</h2>\n"
           "<p>按上面的原理，从低地址写下 int32 的四个字节，再按对照。本机是小端。float32 的符号、阶码和尾数在对照后给出。本机执行仍只核对 int 的 -1，以及 is_iec559 为真时的 1.0f。</p>\n"
           "<label>int32 <input id=\"cpu-int\" type=\"number\" value=\"-1\"></label>\n"
           "<label>预期字节 <input id=\"cpu-int-guess\" type=\"text\" value=\"\"></label>\n"
           "<label>float32 <input id=\"cpu-float\" type=\"number\" value=\"1\" step=\"any\"></label>\n"
           "<p><button type=\"button\" class=\"plain\" id=\"cpu-check\">对照</button></p>\n"
           "<p id=\"cpu-int-bytes\" hidden></p>\n"
           "<p id=\"cpu-float-bits\" hidden></p>\n"
           "</section>\n";
  }
  if (id == "STO-002") {
    return "<section class=\"lab\"><h2>填数观察</h2>\n"
           "<p>按上面的原理写下起始偏移，再按对照。本机执行使用 sysconf 得到的页长，页 0 写入 0xA1，页 1 写入 0xB2。</p>\n"
           "<label>页长 <input id=\"page-size\" type=\"number\" value=\"4096\" min=\"1\"></label>\n"
           "<label>页号 <input id=\"page-no\" type=\"number\" value=\"1\" min=\"0\"></label>\n"
           "<label>预期偏移 <input id=\"page-guess\" type=\"number\"></label>\n"
           "<p><button type=\"button\" class=\"plain\" id=\"page-check\">对照</button></p>\n"
           "<p id=\"page-off\" hidden></p>\n"
           "</section>\n";
  }
  if (id == "OS-002") {
    return "<section class=\"lab\"><h2>填数观察</h2>\n"
           "<p>按上面的原理写下父进程应读到的值，再按对照。本机执行里，fork 前的值是 1，子进程把它改成 2。</p>\n"
           "<label>fork 前的值 <input id=\"fork-before\" type=\"number\" value=\"1\"></label>\n"
           "<label>子进程写入 <input id=\"fork-child\" type=\"number\" value=\"2\"></label>\n"
           "<label>父进程应读到 <input id=\"fork-guess\" type=\"number\"></label>\n"
           "<p><button type=\"button\" class=\"plain\" id=\"fork-check\">对照</button></p>\n"
           "<p id=\"fork-result\" hidden></p>\n"
           "</section>\n";
  }
  if (id == "AT-001") {
    return "<section class=\"lab\"><h2>填数观察</h2>\n"
           "<p>按上面的原理写下互斥量保护下的计数，再按对照。本机执行使用 4 个线程，每个线程递增 1000 次。未加锁的递增不在本节运行。</p>\n"
           "<label>线程数 <input id=\"count-threads\" type=\"number\" value=\"4\" min=\"1\"></label>\n"
           "<label>每线程递增次数 <input id=\"count-each\" type=\"number\" value=\"1000\" min=\"0\"></label>\n"
           "<label>互斥量保护下的计数 <input id=\"count-guess\" type=\"number\"></label>\n"
           "<p><button type=\"button\" class=\"plain\" id=\"count-check\">对照</button></p>\n"
           "<p id=\"count-result\" hidden></p>\n"
           "</section>\n";
  }
  return {};
}

void write_course_page(const std::string& root, const std::vector<Lesson>& lessons, const Registry& registry,
                       const std::map<std::string, std::string>& figures, const LessonCopy& copy,
                       const std::string& fresh_id, const std::vector<RunView>& fresh_runs,
                       const std::string& run_token) {
  const std::string directory = join_path(root, "artifacts/lessons");
  create_directories(directory);
  remove_split_pages(directory);
  std::ostringstream page;
  const char* layers[] = {"基础", "进阶", "方向"};
  page << "<header><a href=\"#course\">目录</a><nav class=\"path\">\n";
  for (const char* layer : layers) {
    for (const Lesson& lesson : lessons) {
      if (lesson.layer == layer) {
        page << "<a data-layer=\"" << layer << "\" href=\"#" << html_escape(lesson.id) << "\">" << layer << "</a>\n";
        break;
      }
    }
  }
  page << "</nav><label>跳到 <select id=\"jump\">\n";
  for (const char* layer : layers) {
    bool opened = false;
    for (std::size_t index = 0; index < lessons.size(); ++index) {
      const Lesson& lesson = lessons[index];
      if (lesson.layer != layer) {
        continue;
      }
      if (!opened) {
        page << "<optgroup label=\"" << layer << "\">\n";
        opened = true;
      }
      page << "<option value=\"" << html_escape(lesson.id) << "\">" << (index + 1) << " " << html_escape(lesson.title)
           << "</option>\n";
    }
    if (opened) {
      page << "</optgroup>\n";
    }
  }
  page << "</select></label></header>\n<div class=\"layout\">\n<aside>\n<a href=\"#course\">课程编排</a>\n";
  for (const char* layer : layers) {
    bool opened = false;
    std::string group;
    for (std::size_t index = 0; index < lessons.size(); ++index) {
      const Lesson& lesson = lessons[index];
      if (lesson.layer != layer) {
        continue;
      }
      if (!opened) {
        page << "<details" << (std::string(layer) == "基础" ? " open" : "") << "><summary>" << layer
             << "</summary>\n";
        opened = true;
        group.clear();
      }
      if (lesson.group != group) {
        group = lesson.group;
        page << "<p>" << html_escape(group) << "</p>\n";
      }
      page << "<a href=\"#" << html_escape(lesson.id) << "\">" << (index + 1) << " " << html_escape(lesson.title)
           << "</a>\n";
    }
    if (opened) {
      page << "</details>\n";
    }
  }
  page << "</aside>\n<main id=\"stage\">\n<article id=\"course\">\n<h1>课程编排</h1>\n"
          "<p>课程分为基础、进阶、方向。基础包括对象复制与移动、整数和浮点的内存表示、虚拟地址与所有权、进程与线程、互斥。进阶接在互斥之后，依次为其他锁、内存序、分配器、编译器改写、可执行文件与调用约定、缓存访问、协程。方向按页与日志、连接与队列、命名空间、访问控制、采样和帧、步骤和工具分列。</p>\n"
          "<p>一节正文的顺序是背景、标题、重点、图、名词、规则、成立的事实、本机演示、证明到哪一层。背景写在标题之前。重点在标题下。背景写该机制所在层次中的处理过程及可观察结果。规则给出 C++ 标准、ABI 或相应操作系统接口中的规定。成立的事实给出本程序的核对结果。证明到哪一层列出本次核对不覆盖的范围。OBJ-001、CPU-001、OS-002、AT-001、STO-002 在规则之后设有填数。本机标准输出在「对照观测」之后显示。一组说明、机制与程序位于折叠区。</p>\n"
          "<p>同一层内更换分组时，导航为下一事实，并标出组名。更换层次时，导航为进入下一层。方向一列的末节，导航为另一方向。</p>\n"
          "</article>\n";
  for (std::size_t index = 0; index < lessons.size(); ++index) {
    const Lesson& lesson = lessons[index];
    const Manifest& manifest = registry.get(lesson.id);
    const std::string readme = readme_text(manifest);
    const std::string checks = markdown_section(readme, "程序实际检查的内容");
    const std::string limit = without_runner_notice(markdown_section(readme, "不由本实验断言的内容"));
    const auto figure = figures.find(lesson.id);
    const bool fresh = lesson.id == fresh_id && !fresh_runs.empty();
    int layer_count = 0;
    int layer_index = 0;
    for (std::size_t cursor = 0; cursor < lessons.size(); ++cursor) {
      if (lessons[cursor].layer != lesson.layer) {
        continue;
      }
      ++layer_count;
      if (cursor <= index) {
        layer_index = layer_count;
      }
    }
    page << "<article id=\"" << html_escape(lesson.id) << "\" data-layer=\"" << html_escape(lesson.layer) << "\">\n";
    page << "<p class=\"meta\">" << html_escape(lesson.layer) << " 第 " << layer_index << " 节 / " << layer_count
         << " · 全课第 " << (index + 1) << " 节 / " << lessons.size() << " · " << html_escape(lesson.group)
         << "</p>\n";
    const auto brief = copy.briefs.find(lesson.id);
    const bool has_brief = brief != copy.briefs.end();
    const bool show_fact = has_brief && !brief->second.principle.empty() &&
                           brief->second.principle != brief->second.mechanism;
    const std::string focus = has_brief && !brief->second.focus.empty() ? brief->second.focus : lesson.title;
    if (has_brief && !brief->second.background.empty()) {
      page << "<section class=\"lead\"><h2>背景</h2>\n" << paragraph_html(brief->second.background) << "</section>\n";
    }
    page << "<h1>" << html_escape(lesson.title) << "</h1>\n";
    page << "<p class=\"ask\">" << html_escape(focus) << "</p>\n";
    page << "<ol class=\"chain\">\n";
    const char* previous_label = nullptr;
    if (const Lesson* previous = track_neighbor(lessons, index, -1, &previous_label)) {
      page << "<li><span>" << previous_label << "</span><a href=\"#" << html_escape(previous->id) << "\">"
           << html_escape(previous->title) << "</a>";
      if (previous->group != lesson.group || previous->layer != lesson.layer) {
        page << "<p class=\"where\">" << html_escape(previous->layer) << " · " << html_escape(previous->group)
             << "</p>";
      }
      page << "</li>\n";
    }
    page << "<li class=\"now\"><span>这一事实</span><strong>" << html_escape(lesson.title) << "</strong></li>\n";
    const char* next_label = nullptr;
    if (const Lesson* next = track_neighbor(lessons, index, 1, &next_label)) {
      page << "<li><span>" << next_label << "</span><a href=\"#" << html_escape(next->id) << "\">" << html_escape(next->title)
           << "</a>";
      if (next->group != lesson.group || next->layer != lesson.layer) {
        page << "<p class=\"where\">" << html_escape(next->layer) << " · " << html_escape(next->group) << "</p>";
      }
      page << "</li>\n";
    }
    page << "</ol>\n";
    const std::string figure_text = figure == figures.end() ? std::string() : figure->second;
    if (!figure_text.empty()) {
      page << figure_html(figure_text, "");
    }
    const std::string frames = motion_caption(lesson.id);
    page << motion_html(root, lesson.id, frames.empty() ? lesson.title : frames);
    if (has_brief && !brief->second.term.empty()) {
      page << "<p class=\"term\">" << html_escape(brief->second.term) << "</p>\n";
    }
    if (!manifest.prerequisites.empty()) {
      page << "<p>先看";
      for (const std::string& required : manifest.prerequisites) {
        const Lesson* required_lesson = find_lesson(lessons, required);
        const std::string& label = required_lesson == nullptr ? required : required_lesson->title;
        page << " <a href=\"#" << html_escape(required) << "\">" << html_escape(label) << "</a>";
      }
      page << "。</p>\n";
    }
    const bool group_start = index == 0 || lessons[index - 1].group != lesson.group || lessons[index - 1].layer != lesson.layer;
    const auto group_text = copy.groups.find(lesson.group);
    const bool has_group = group_start && group_text != copy.groups.end() && !group_text->second.empty();
    if (has_group) {
      page << "<details class=\"more\"><summary>这一组</summary>\n" << paragraph_html(group_text->second)
           << "</details>\n";
    }
    auto emit_layer = [&](const char* name, const std::string& body) {
      if (body.empty()) {
        return;
      }
      page << "<div class=\"layer\"><h3>" << name << "</h3>\n" << paragraph_html(body) << "</div>\n";
    };
    page << "<section><h2>原理</h2>\n";
    if (has_brief) {
      emit_layer("规则", brief->second.language);
      if (!show_fact && !brief->second.mechanism.empty()) {
        page << paragraph_html(brief->second.mechanism);
      }
      page << interactive_html(lesson.id);
    }
    page << "</section>\n" << expect_html(lesson.id, focus);
    if (show_fact) {
      page << "<section><h2>成立的事实</h2>\n" << paragraph_html(brief->second.principle) << "</section>\n";
    }
    page << "<section><h2>本机演示</h2>\n"
         << demo_html(lesson.id, show_fact ? std::string() : checks, figure_text, fresh ? fresh_runs : std::vector<RunView>{},
                      fresh ? std::string() : latest_run_directory(root, lesson.id), run_token)
         << "</section>\n<section><h2>证明到哪一层</h2>\n";
    if (!limit.empty()) {
      page << paragraph_html(limit);
    }
    page << "<p>对照：" << html_escape(lesson.diagnose) << "</p>\n";
    page << "</section>\n";
    const bool has_program = has_brief && !brief->second.program.empty();
    const bool folded_checks = show_fact && !checks.empty();
    const bool folded_mechanism = show_fact && !brief->second.mechanism.empty();
    if (has_program || folded_checks || folded_mechanism) {
      page << "<details class=\"more\"><summary>机制和程序</summary>\n";
      if (folded_mechanism) {
        emit_layer("机制", brief->second.mechanism);
      }
      if (has_program) {
        emit_layer("程序", brief->second.program);
      }
      if (folded_checks) {
        emit_layer("核对", checks);
      }
      page << "</details>\n";
    }
    page << "<details class=\"source\"><summary>源码</summary>\n" << sources_html(manifest.directory)
         << "</details>\n</article>\n";
  }
  page << "</main></div>\n<script>\n"
          "function show(id){var arts=document.querySelectorAll('article');var found=false;var layer='';"
          "for(var i=0;i<arts.length;i++){var on=arts[i].id===id;arts[i].style.display=on?'block':'none';if(on){found=true;layer=arts[i].getAttribute('data-layer')||'';}}"
          "if(!found&&arts.length){arts[0].style.display='block';id=arts[0].id;layer=arts[0].getAttribute('data-layer')||'';}"
          "var links=document.querySelectorAll('aside a');"
          "for(var j=0;j<links.length;j++){links[j].className=links[j].getAttribute('href')==='#'+id?'active':'';}"
          "var path=document.querySelectorAll('nav.path a');"
          "for(var k=0;k<path.length;k++){path[k].className=path[k].getAttribute('data-layer')===layer?'on':'';}"
          "var jump=document.getElementById('jump');if(jump){var has=false;for(var o=0;o<jump.options.length;o++){if(jump.options[o].value===id)has=true;}if(has)jump.value=id;}"
          "if(location.hash!=='#'+id)history.replaceState(null,'','#'+id);}\n"
          "document.getElementById('jump').addEventListener('change',function(){show(this.value);});\n"
          "window.addEventListener('hashchange',function(){show(location.hash.slice(1));});\n"
          "function hexByte(n){var s=(n&255).toString(16);return s.length<2?'0'+s:s;}\n"
          "function expectKey(id){return 'syslab-expect-'+id;}\n"
          "function sameNum(left,right){return left!==''&&right!==''&&Number(left)===Number(right);}\n"
          "function normBytes(text){return text.trim().toLowerCase().replace(/0x/g,'').split(/[\\s,]+/).filter(Boolean).map(function(part){return part.length===1?'0'+part:part;}).join(' ');}\n"
          "document.addEventListener('input',function(ev){var t=ev.target;if(t&&t.classList&&t.classList.contains('expect'))sessionStorage.setItem(expectKey(t.getAttribute('data-lesson')),t.value);});\n"
          "document.addEventListener('submit',function(ev){var form=ev.target;if(!form||!form.getAttribute)return;var id=form.getAttribute('data-lesson');if(!id)return;var area=document.getElementById('expect-'+id);if(area)sessionStorage.setItem(expectKey(id),area.value);});\n"
          "document.addEventListener('click',function(ev){\n"
          "var node=ev.target;if(!node||!node.getAttribute)return;\n"
          "var reveal=node.getAttribute('data-reveal');\n"
          "if(reveal){var obs=document.getElementById('obs-'+reveal);if(obs)obs.hidden=false;var note=document.getElementById('expect-note-'+reveal);var area=document.getElementById('expect-'+reveal);if(note)note.textContent=(area&&area.value.trim())?'下面是本机观测，与写下的预期对照。':'尚未写下预期。下面仍列出本机观测。';return;}\n"
          "if(node.id==='obj-check'){var at=document.getElementById('obj-copy-at').value,to=document.getElementById('obj-copy-to').value,gs=document.getElementById('obj-guess-shallow').value,gd=document.getElementById('obj-guess-deep').value,box=document.getElementById('obj-result');box.hidden=false;box.textContent=(gs===''||gd===''?'尚未写下两个读数。':'')+'按规则，浅拷贝读到 '+to+(sameNum(gs,to)?'，与预期一致。':'，与预期不一致。')+'深拷贝读到 '+at+(sameNum(gd,at)?'，与预期一致。':'，与预期不一致。');}\n"
          "if(node.id==='cpu-check'){var iv=document.getElementById('cpu-int'),guess=document.getElementById('cpu-int-guess'),ib=document.getElementById('cpu-int-bytes'),fv=document.getElementById('cpu-float'),fb=document.getElementById('cpu-float-bits');var n=Number(iv.value);ib.hidden=false;fb.hidden=false;if(!Number.isFinite(n)||n<-2147483648||n>2147483647){ib.textContent='需要 int32 范围内的整数。';}else{var view=new DataView(new ArrayBuffer(4));view.setInt32(0,n,true);var bytes=[];for(var i=0;i<4;i++)bytes.push(hexByte(view.getUint8(i)));var got=bytes.join(' ');var wrote=guess.value.trim();ib.textContent='从低地址到高地址：'+got+(wrote?(normBytes(wrote)===got?'，与预期一致。':'，与预期不一致。'):'。尚未写下预期字节。');}var f=Number(fv.value);if(!Number.isFinite(f)){fb.textContent='需要有限的 float32。';}else{var bitsView=new DataView(new ArrayBuffer(4));bitsView.setFloat32(0,f,true);var bits=bitsView.getUint32(0,true);var floatBytes=[];for(var j=0;j<4;j++)floatBytes.push(hexByte(bitsView.getUint8(j)));fb.textContent='从低地址到高地址：'+floatBytes.join(' ')+'。位型 0x'+bits.toString(16)+'，符号 '+(bits>>>31)+'，阶码 '+((bits>>>23)&255)+'，尾数 '+(bits&0x7fffff)+'。';}}\n"
          "if(node.id==='page-check'){var size=Number(document.getElementById('page-size').value),no=Number(document.getElementById('page-no').value),guessOff=document.getElementById('page-guess').value,po=document.getElementById('page-off');po.hidden=false;if(!Number.isFinite(size)||size<1||!Number.isFinite(no)||no<0){po.textContent='页长需要正数，页号需要非负整数。';}else{var off=no*size;po.textContent='起始偏移 '+off+'。区间 ['+off+', '+(off+size)+')。'+(guessOff===''?'尚未写下预期偏移。':(sameNum(guessOff,off)?'与预期一致。':'与预期不一致。'));}}\n"
          "if(node.id==='fork-check'){var before=document.getElementById('fork-before').value,child=document.getElementById('fork-child').value,guess=document.getElementById('fork-guess').value,box=document.getElementById('fork-result');box.hidden=false;if(before===''){box.textContent='需要 fork 前的值。';}else{box.textContent=(guess===''?'尚未写下父进程的读数。':'')+'按规则，父进程读到 '+before+(sameNum(guess,before)?'，与预期一致。':'，与预期不一致。')+'子进程写入的 '+child+' 不出现在父进程里。';}}\n"
          "if(node.id==='count-check'){var threads=Number(document.getElementById('count-threads').value),each=Number(document.getElementById('count-each').value),guess=document.getElementById('count-guess').value,box=document.getElementById('count-result');box.hidden=false;if(!Number.isInteger(threads)||threads<1||!Number.isInteger(each)||each<0){box.textContent='线程数需要正整数，每线程次数需要非负整数。';}else{var total=threads*each;box.textContent='互斥量保护下的计数是 '+total+'。'+(guess===''?'尚未写下预期计数。':(sameNum(guess,total)?'与预期一致。':'与预期不一致。'))+'本节不运行未加锁的递增。';}}\n"
          "});\n"
          "var areas=document.querySelectorAll('textarea.expect');for(var ei=0;ei<areas.length;ei++){var saved=sessionStorage.getItem(expectKey(areas[ei].getAttribute('data-lesson')));if(saved!==null)areas[ei].value=saved;}\n"
          "show(location.hash.slice(1));\n"
          "</script>\n";
  const std::string page_path = join_path(directory, "index.html");
  const std::string temporary = page_path + ".tmp";
  write_file(temporary, page_shell("目录", page.str()));
  if (std::rename(temporary.c_str(), page_path.c_str()) != 0) {
    throw Error(Error::Code::Io, "无法更新课程页面");
  }
}

}  // namespace syslab
