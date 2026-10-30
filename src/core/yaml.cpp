#include "syslab/core/yaml.hpp"

#include "syslab/core/error.hpp"

#include <cctype>

namespace syslab {
namespace {

struct RawLine {
  int number = 0;
  int indent = 0;
  std::string text;
};

std::string trim_copy(const std::string& text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

[[noreturn]] void parse_error(const std::string& origin, int line, const std::string& message) {
  throw Error(Error::Code::Parse, origin + ":" + std::to_string(line) + ": " + message);
}

std::string strip_comment(const std::string& line, int number, const std::string& origin) {
  bool quoted = false;
  char quote = 0;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];
    if (character == '\t') {
      parse_error(origin, number, "tabs are not allowed; use two-space indentation");
    }
    if (quoted) {
      if (character == '\\' && quote == '"' && index + 1 < line.size()) {
        ++index;
        continue;
      }
      if (character == quote) {
        quoted = false;
      }
      continue;
    }
    if (character == '"' || character == '\'') {
      quoted = true;
      quote = character;
      continue;
    }
    if (character == '#') {
      return line.substr(0, index);
    }
  }
  return line;
}

std::vector<RawLine> split_lines(const std::string& text, const std::string& origin) {
  std::vector<RawLine> lines;
  std::size_t cursor = 0;
  int number = 1;
  while (cursor <= text.size()) {
    std::size_t end = text.find('\n', cursor);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string raw = text.substr(cursor, end - cursor);
    if (!raw.empty() && raw.back() == '\r') {
      raw.pop_back();
    }
    raw = strip_comment(raw, number, origin);
    std::size_t indent = 0;
    while (indent < raw.size() && raw[indent] == ' ') {
      ++indent;
    }
    const std::string body = trim_copy(raw.substr(indent));
    if (!body.empty() && body != "---") {
      if (indent % 2 != 0) {
        parse_error(origin, number, "indentation must be a multiple of two spaces");
      }
      RawLine line;
      line.number = number;
      line.indent = static_cast<int>(indent);
      line.text = body;
      lines.push_back(std::move(line));
    }
    if (end == text.size()) {
      break;
    }
    cursor = end + 1;
    ++number;
  }
  return lines;
}

bool is_sequence_item(const std::string& text) {
  return text == "-" || (text.size() > 1 && text[0] == '-' && text[1] == ' ');
}

std::string sequence_rest(const std::string& text) {
  if (text == "-") {
    return {};
  }
  return trim_copy(text.substr(1));
}

std::size_t colon_position(const std::string& text) {
  bool quoted = false;
  char quote = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (quoted) {
      if (character == '\\' && quote == '"' && index + 1 < text.size()) {
        ++index;
        continue;
      }
      if (character == quote) {
        quoted = false;
      }
      continue;
    }
    if (character == '"' || character == '\'') {
      quoted = true;
      quote = character;
      continue;
    }
    if (character == ':') {
      return index;
    }
  }
  return std::string::npos;
}

std::string unquote(const std::string& text, const std::string& origin, int line) {
  if (text.size() < 2) {
    return text;
  }
  const char quote = text.front();
  if ((quote != '"' && quote != '\'') || text.back() != quote) {
    return text;
  }
  std::string value;
  for (std::size_t index = 1; index + 1 < text.size(); ++index) {
    if (quote == '"' && text[index] == '\\') {
      if (index + 1 >= text.size() - 1) {
        parse_error(origin, line, "unterminated escape in scalar");
      }
      const char escaped = text[++index];
      switch (escaped) {
        case 'n':
          value.push_back('\n');
          break;
        case 't':
          value.push_back('\t');
          break;
        case '"':
        case '\\':
          value.push_back(escaped);
          break;
        default:
          parse_error(origin, line, "unsupported scalar escape");
      }
    } else {
      value.push_back(text[index]);
    }
  }
  return value;
}

}  // namespace

struct YamlParser {
  std::vector<RawLine> lines;
  std::string origin;

  Yaml parse_document() {
    std::size_t index = 0;
    if (lines.empty()) {
      return Yaml();
    }
    Yaml root = parse_node(index, lines.front().indent);
    if (index != lines.size()) {
      parse_error(origin, lines[index].number, "unexpected content after the root value");
    }
    return root;
  }

  Yaml parse_node(std::size_t& index, int indent) {
    if (index >= lines.size() || lines[index].indent < indent) {
      return Yaml();
    }
    if (lines[index].indent > indent) {
      parse_error(origin, lines[index].number, "indentation jumps over the expected level");
    }
    if (is_sequence_item(lines[index].text)) {
      return parse_sequence(index, indent);
    }
    return parse_map(index, indent);
  }

  Yaml parse_sequence(std::size_t& index, int indent) {
    Yaml sequence;
    sequence.kind_ = Yaml::Kind::Sequence;
    while (index < lines.size() && lines[index].indent == indent && is_sequence_item(lines[index].text)) {
      const RawLine item = lines[index];
      const std::string rest = sequence_rest(item.text);
      ++index;
      if (rest.empty()) {
        if (index >= lines.size() || lines[index].indent <= indent) {
          sequence.sequence_.push_back(Yaml());
        } else {
          sequence.sequence_.push_back(parse_node(index, lines[index].indent));
        }
        continue;
      }
      if (colon_position(rest) != std::string::npos && !starts_with_quote(rest)) {
        Yaml mapping;
        mapping.kind_ = Yaml::Kind::Map;
        add_mapping_entry(mapping, rest, index, indent + 2, item.number);
        while (index < lines.size() && lines[index].indent >= indent + 2 &&
               !is_sequence_item(lines[index].text)) {
          if (lines[index].indent != indent + 2) {
            parse_error(origin, lines[index].number, "mapping entry has inconsistent indentation");
          }
          const std::string entry = lines[index].text;
          const int line = lines[index].number;
          ++index;
          add_mapping_entry(mapping, entry, index, indent + 4, line);
        }
        sequence.sequence_.push_back(std::move(mapping));
      } else {
        sequence.sequence_.push_back(scalar_node(unquote(rest, origin, item.number)));
      }
    }
    return sequence;
  }

  Yaml parse_map(std::size_t& index, int indent) {
    Yaml mapping;
    mapping.kind_ = Yaml::Kind::Map;
    if (index < lines.size() && is_sequence_item(lines[index].text)) {
      parse_error(origin, lines[index].number, "expected a mapping");
    }
    while (index < lines.size() && lines[index].indent == indent) {
      if (is_sequence_item(lines[index].text)) {
        break;
      }
      const std::string entry = lines[index].text;
      const int line = lines[index].number;
      ++index;
      add_mapping_entry(mapping, entry, index, indent + 2, line);
    }
    return mapping;
  }

  void add_mapping_entry(Yaml& mapping, const std::string& text, std::size_t& index, int child_indent,
                         int line) {
    const std::size_t colon = colon_position(text);
    if (colon == std::string::npos) {
      parse_error(origin, line, "mapping entry requires a colon");
    }
    const std::string key = trim_copy(text.substr(0, colon));
    std::string value = trim_copy(text.substr(colon + 1));
    if (key.empty() || starts_with_quote(key)) {
      parse_error(origin, line, "mapping key must be a plain scalar");
    }
    for (const auto& existing : mapping.map_) {
      if (existing.first == key) {
        parse_error(origin, line, "duplicate key '" + key + "'");
      }
    }
    Yaml child;
    if (value == "[]") {
      child.kind_ = Yaml::Kind::Sequence;
    } else if (value == "~" || value == "null") {
      child.kind_ = Yaml::Kind::Null;
    } else if (!value.empty()) {
      if (value.front() == '[' || value.front() == '{') {
        parse_error(origin, line, "only the empty inline sequence [] is supported");
      }
      child = scalar_node(unquote(value, origin, line));
    } else if (index < lines.size() && lines[index].indent >= child_indent) {
      child = parse_node(index, lines[index].indent);
    } else {
      child = scalar_node("");
    }
    mapping.map_.emplace_back(key, std::move(child));
  }

  static bool starts_with_quote(const std::string& text) {
    return !text.empty() && (text.front() == '"' || text.front() == '\'');
  }

  static Yaml scalar_node(std::string text) {
    Yaml node;
    node.kind_ = Yaml::Kind::Scalar;
    node.scalar_ = std::move(text);
    return node;
  }
};

Yaml Yaml::parse(const std::string& text, const std::string& origin) {
  YamlParser parser;
  parser.lines = split_lines(text, origin);
  parser.origin = origin;
  return parser.parse_document();
}

const std::string& Yaml::as_scalar() const {
  if (kind_ != Kind::Scalar) {
    throw Error(Error::Code::Validation, "YAML node is not a scalar");
  }
  return scalar_;
}

long long Yaml::as_integer() const {
  const std::string& text = as_scalar();
  if (text.empty()) {
    throw Error(Error::Code::Validation, "YAML integer is empty");
  }
  std::size_t index = 0;
  if (text[0] == '-' || text[0] == '+') {
    index = 1;
  }
  if (index >= text.size()) {
    throw Error(Error::Code::Validation, "YAML integer is incomplete");
  }
  for (; index < text.size(); ++index) {
    if (!std::isdigit(static_cast<unsigned char>(text[index]))) {
      throw Error(Error::Code::Validation, "YAML integer contains non-digit characters");
    }
  }
  try {
    std::size_t consumed = 0;
    const long long value = std::stoll(text, &consumed, 10);
    if (consumed != text.size()) {
      throw Error(Error::Code::Validation, "YAML integer is incomplete");
    }
    return value;
  } catch (const Error&) {
    throw;
  } catch (const std::exception&) {
    throw Error(Error::Code::Validation, "YAML integer is out of range");
  }
}

bool Yaml::as_bool() const {
  const std::string& text = as_scalar();
  if (text == "true") {
    return true;
  }
  if (text == "false") {
    return false;
  }
  throw Error(Error::Code::Validation, "YAML boolean must be true or false");
}

const std::vector<Yaml>& Yaml::as_sequence() const {
  if (kind_ != Kind::Sequence) {
    throw Error(Error::Code::Validation, "YAML node is not a sequence");
  }
  return sequence_;
}

const Yaml* Yaml::find(const std::string& key) const {
  if (kind_ != Kind::Map) {
    return nullptr;
  }
  for (const auto& entry : map_) {
    if (entry.first == key) {
      return &entry.second;
    }
  }
  return nullptr;
}

const Yaml& Yaml::require(const std::string& key) const {
  const Yaml* value = find(key);
  if (value == nullptr) {
    throw Error(Error::Code::Validation, "missing YAML key '" + key + "'");
  }
  return *value;
}

const std::vector<std::pair<std::string, Yaml>>& Yaml::items() const {
  if (kind_ != Kind::Map) {
    throw Error(Error::Code::Validation, "YAML node is not a mapping");
  }
  return map_;
}

}  // namespace syslab
