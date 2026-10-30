#include "syslab/core/json.hpp"

#include "syslab/core/error.hpp"

#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace syslab {
namespace {

class Parser {
 public:
  Parser(const std::string& text) : text_(text) {}

  Json parse_document() {
    skip();
    Json value = parse_value();
    skip();
    if (index_ != text_.size()) {
      fail("trailing data after JSON value");
    }
    return value;
  }

 private:
  const std::string& text_;
  std::size_t index_ = 0;

  [[noreturn]] void fail(const std::string& message) const {
    throw Error(Error::Code::Parse, "JSON parse error at offset " + std::to_string(index_) + ": " + message);
  }

  void skip() {
    while (index_ < text_.size() &&
           (text_[index_] == ' ' || text_[index_] == '\n' || text_[index_] == '\r' || text_[index_] == '\t')) {
      ++index_;
    }
  }

  char peek() const {
    if (index_ >= text_.size()) {
      fail("unexpected end of JSON");
    }
    return text_[index_];
  }

  char get() {
    char value = peek();
    ++index_;
    return value;
  }

  Json parse_value() {
    skip();
    const char head = peek();
    if (head == '{') {
      return parse_object();
    }
    if (head == '[') {
      return parse_array();
    }
    if (head == '"') {
      return Json::string(parse_string());
    }
    if (head == 't' || head == 'f') {
      return Json::boolean(parse_bool());
    }
    if (head == 'n') {
      parse_literal("null");
      return Json::null();
    }
    if (head == '-' || (head >= '0' && head <= '9')) {
      return parse_number();
    }
    fail("unexpected JSON token");
  }

  Json parse_object() {
    get();
    Json object = Json::object();
    skip();
    if (peek() == '}') {
      get();
      return object;
    }
    while (true) {
      skip();
      if (peek() != '"') {
        fail("object key must be a string");
      }
      const std::string key = parse_string();
      skip();
      if (get() != ':') {
        fail("expected colon after object key");
      }
      object.set(key, parse_value());
      skip();
      const char delimiter = get();
      if (delimiter == '}') {
        break;
      }
      if (delimiter != ',') {
        fail("expected comma or end of object");
      }
    }
    return object;
  }

  Json parse_array() {
    get();
    Json array = Json::array();
    skip();
    if (peek() == ']') {
      get();
      return array;
    }
    while (true) {
      array.push(parse_value());
      skip();
      const char delimiter = get();
      if (delimiter == ']') {
        break;
      }
      if (delimiter != ',') {
        fail("expected comma or end of array");
      }
    }
    return array;
  }

  std::string parse_string() {
    if (get() != '"') {
      fail("expected string");
    }
    std::string value;
    while (true) {
      if (index_ >= text_.size()) {
        fail("unterminated string");
      }
      const char character = get();
      if (character == '"') {
        break;
      }
      if (character == '\\') {
        if (index_ >= text_.size()) {
          fail("unterminated escape");
        }
        const char escaped = get();
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            value.push_back(escaped);
            break;
          case 'b':
            value.push_back('\b');
            break;
          case 'f':
            value.push_back('\f');
            break;
          case 'n':
            value.push_back('\n');
            break;
          case 'r':
            value.push_back('\r');
            break;
          case 't':
            value.push_back('\t');
            break;
          case 'u':
            append_unicode(&value);
            break;
          default:
            fail("unsupported string escape");
        }
      } else if (static_cast<unsigned char>(character) < 0x20) {
        fail("raw control character in string");
      } else {
        value.push_back(character);
      }
    }
    return value;
  }

  void append_unicode(std::string* value) {
    if (index_ + 4 > text_.size()) {
      fail("short unicode escape");
    }
    int code = 0;
    for (int nibble = 0; nibble < 4; ++nibble) {
      const char hex = get();
      code <<= 4;
      if (hex >= '0' && hex <= '9') {
        code += hex - '0';
      } else if (hex >= 'a' && hex <= 'f') {
        code += hex - 'a' + 10;
      } else if (hex >= 'A' && hex <= 'F') {
        code += hex - 'A' + 10;
      } else {
        fail("invalid unicode escape");
      }
    }
    if (code >= 0xD800 && code <= 0xDFFF) {
      fail("surrogate unicode escapes are not accepted");
    }
    append_utf8(value, code);
  }

  static void append_utf8(std::string* value, int code) {
    if (code < 0x80) {
      value->push_back(static_cast<char>(code));
    } else if (code < 0x800) {
      value->push_back(static_cast<char>(0xC0 | (code >> 6)));
      value->push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
      value->push_back(static_cast<char>(0xE0 | (code >> 12)));
      value->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      value->push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
  }

  bool parse_bool() {
    if (text_.compare(index_, 4, "true") == 0) {
      index_ += 4;
      return true;
    }
    if (text_.compare(index_, 5, "false") == 0) {
      index_ += 5;
      return false;
    }
    fail("invalid boolean");
  }

  void parse_literal(const char* literal) {
    const std::size_t length = std::char_traits<char>::length(literal);
    if (text_.compare(index_, length, literal) != 0) {
      fail(std::string("expected ") + literal);
    }
    index_ += length;
  }

  Json parse_number() {
    const std::size_t start = index_;
    if (peek() == '-') {
      get();
    }
    if (!std::isdigit(static_cast<unsigned char>(peek()))) {
      fail("invalid number");
    }
    if (peek() == '0') {
      get();
    } else {
      while (index_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        ++index_;
      }
    }
    bool real = false;
    if (index_ < text_.size() && text_[index_] == '.') {
      real = true;
      get();
      if (index_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        fail("invalid fraction");
      }
      while (index_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        ++index_;
      }
    }
    if (index_ < text_.size() && (text_[index_] == 'e' || text_[index_] == 'E')) {
      real = true;
      get();
      if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
        get();
      }
      if (index_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        fail("invalid exponent");
      }
      while (index_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        ++index_;
      }
    }
    const std::string token = text_.substr(start, index_ - start);
    if (!real) {
      try {
        std::size_t consumed = 0;
        const long long value = std::stoll(token, &consumed, 10);
        if (consumed != token.size()) {
          fail("invalid integer");
        }
        return Json::integer(value);
      } catch (const std::exception&) {
        fail("integer out of range");
      }
    }
    try {
      std::size_t consumed = 0;
      const double value = std::stod(token, &consumed);
      if (consumed != token.size()) {
        fail("invalid real");
      }
      return Json::real(value);
    } catch (const std::exception&) {
      fail("real out of range");
    }
  }
};

void append_escaped(std::string* out, const std::string& value) {
  out->push_back('"');
  for (unsigned char character : value) {
    switch (character) {
      case '"':
        out->append("\\\"");
        break;
      case '\\':
        out->append("\\\\");
        break;
      case '\b':
        out->append("\\b");
        break;
      case '\f':
        out->append("\\f");
        break;
      case '\n':
        out->append("\\n");
        break;
      case '\r':
        out->append("\\r");
        break;
      case '\t':
        out->append("\\t");
        break;
      default:
        if (character < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out->append("\\u00");
          out->push_back(hex[character >> 4]);
          out->push_back(hex[character & 0x0F]);
        } else {
          out->push_back(static_cast<char>(character));
        }
    }
  }
  out->push_back('"');
}

}  // namespace

Json Json::null() { return Json(); }

Json Json::boolean(bool value) {
  Json json;
  json.type_ = Type::Bool;
  json.bool_ = value;
  return json;
}

Json Json::integer(std::int64_t value) {
  Json json;
  json.type_ = Type::Integer;
  json.integer_ = value;
  return json;
}

Json Json::real(double value) {
  if (!std::isfinite(value)) {
    throw Error(Error::Code::InvalidArgument, "JSON real must be finite");
  }
  Json json;
  json.type_ = Type::Real;
  json.real_ = value;
  return json;
}

Json Json::string(std::string value) {
  Json json;
  json.type_ = Type::String;
  json.text_ = std::move(value);
  return json;
}

Json Json::array() {
  Json json;
  json.type_ = Type::Array;
  return json;
}

Json Json::object() {
  Json json;
  json.type_ = Type::Object;
  return json;
}

Json Json::parse(const std::string& text) {
  Parser parser(text);
  return parser.parse_document();
}

Json& Json::push(Json value) {
  if (type_ != Type::Array) {
    throw Error(Error::Code::Logic, "JSON push requires an array");
  }
  array_.push_back(std::move(value));
  return *this;
}

Json& Json::set(const std::string& key, Json value) {
  if (type_ != Type::Object) {
    throw Error(Error::Code::Logic, "JSON set requires an object");
  }
  for (auto& field : object_) {
    if (field.first == key) {
      field.second = std::move(value);
      return *this;
    }
  }
  object_.emplace_back(key, std::move(value));
  return *this;
}

std::string Json::dump(int indent) const {
  std::string out;
  dump_into(&out, indent, 0);
  if (indent > 0) {
    out.push_back('\n');
  }
  return out;
}

bool Json::as_bool() const {
  if (type_ != Type::Bool) {
    throw Error(Error::Code::Logic, "JSON value is not a boolean");
  }
  return bool_;
}

std::int64_t Json::as_integer() const {
  if (type_ != Type::Integer) {
    throw Error(Error::Code::Logic, "JSON value is not an integer");
  }
  return integer_;
}

const std::string& Json::as_string() const {
  if (type_ != Type::String) {
    throw Error(Error::Code::Logic, "JSON value is not a string");
  }
  return text_;
}

const std::vector<Json>& Json::as_array() const {
  if (type_ != Type::Array) {
    throw Error(Error::Code::Logic, "JSON value is not an array");
  }
  return array_;
}

const Json* Json::find(const std::string& key) const {
  if (type_ != Type::Object) {
    return nullptr;
  }
  for (const auto& field : object_) {
    if (field.first == key) {
      return &field.second;
    }
  }
  return nullptr;
}

void Json::dump_into(std::string* out, int indent, int depth) const {
  switch (type_) {
    case Type::Null:
      out->append("null");
      break;
    case Type::Bool:
      out->append(bool_ ? "true" : "false");
      break;
    case Type::Integer:
      out->append(std::to_string(integer_));
      break;
    case Type::Real: {
      std::ostringstream buffer;
      buffer.setf(std::ios::fmtflags(0), std::ios::floatfield);
      buffer.precision(17);
      buffer << real_;
      out->append(buffer.str());
      break;
    }
    case Type::String:
      append_escaped(out, text_);
      break;
    case Type::Array: {
      out->push_back('[');
      if (array_.empty()) {
        out->push_back(']');
        break;
      }
      for (std::size_t i = 0; i < array_.size(); ++i) {
        if (indent > 0) {
          out->push_back('\n');
          out->append(static_cast<std::size_t>((depth + 1) * indent), ' ');
        }
        array_[i].dump_into(out, indent, depth + 1);
        if (i + 1 != array_.size()) {
          out->push_back(',');
        }
      }
      if (indent > 0) {
        out->push_back('\n');
        out->append(static_cast<std::size_t>(depth * indent), ' ');
      }
      out->push_back(']');
      break;
    }
    case Type::Object: {
      out->push_back('{');
      if (object_.empty()) {
        out->push_back('}');
        break;
      }
      for (std::size_t i = 0; i < object_.size(); ++i) {
        if (indent > 0) {
          out->push_back('\n');
          out->append(static_cast<std::size_t>((depth + 1) * indent), ' ');
        }
        append_escaped(out, object_[i].first);
        out->append(indent > 0 ? ": " : ":");
        object_[i].second.dump_into(out, indent, depth + 1);
        if (i + 1 != object_.size()) {
          out->push_back(',');
        }
      }
      if (indent > 0) {
        out->push_back('\n');
        out->append(static_cast<std::size_t>(depth * indent), ' ');
      }
      out->push_back('}');
      break;
    }
  }
}

}  // namespace syslab
