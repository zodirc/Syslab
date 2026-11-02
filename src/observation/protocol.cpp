#include "syslab/observation/protocol.hpp"

namespace syslab {
namespace {

bool valid_basis(const std::string& basis) {
  return basis == "LANG" || basis == "ABI" || basis == "COMPILER" || basis == "OS" || basis == "ISA" ||
         basis == "MICROARCH" || basis == "OBSERVED";
}

bool valid_status(const std::string& status) {
  return status == "pass" || status == "fail" || status == "unsupported";
}

void add_error(ProtocolRecord* record, int line, const std::string& message) {
  ProtocolDiagnostic diagnostic;
  diagnostic.line = line;
  diagnostic.message = message;
  record->errors.push_back(std::move(diagnostic));
}

void parse_result(ProtocolRecord* record, int line, const std::string& body) {
  std::string status;
  std::string reason;
  bool saw_status = false;
  std::size_t cursor = 0;
  while (cursor < body.size()) {
    while (cursor < body.size() && body[cursor] == ' ') {
      ++cursor;
    }
    if (cursor >= body.size()) {
      break;
    }
    const std::size_t space = body.find(' ', cursor);
    const std::string token = body.substr(cursor, space == std::string::npos ? std::string::npos : space - cursor);
    const std::size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0) {
      add_error(record, line, "SYSLAB_RESULT token is missing '='");
    } else {
      const std::string key = token.substr(0, eq);
      const std::string value = token.substr(eq + 1);
      if (key == "status") {
        saw_status = true;
        status = value;
      } else if (key == "reason") {
        reason = value;
      } else {
        add_error(record, line, "SYSLAB_RESULT has unknown field " + key);
      }
    }
    if (space == std::string::npos) {
      break;
    }
    cursor = space + 1;
  }
  if (!saw_status || !valid_status(status)) {
    add_error(record, line, "SYSLAB_RESULT status must be pass, fail, or unsupported");
    return;
  }
  if (!record->status.empty()) {
    add_error(record, line, "SYSLAB_RESULT is repeated");
    return;
  }
  record->status = status;
  record->reason = reason;
}

void parse_line(ProtocolRecord* record, int line, const std::string& text) {
  constexpr const char* kResult = "SYSLAB_RESULT";
  constexpr const char* kMetric = "SYSLAB_METRIC ";
  constexpr const char* kNote = "SYSLAB_NOTE ";
  constexpr const char* kStep = "SYSLAB_STEP ";
  if (text.size() < 7 || text.compare(0, 7, "SYSLAB_") != 0) {
    return;
  }
  if (text == kResult || text.compare(0, 14, "SYSLAB_RESULT ") == 0) {
    const std::string body = text.size() <= 14 ? std::string() : text.substr(14);
    parse_result(record, line, body);
    return;
  }
  if (text.compare(0, 14, kMetric) == 0) {
    const std::string body = text.substr(14);
    const std::size_t eq = body.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= body.size()) {
      add_error(record, line, "SYSLAB_METRIC requires name=value");
      return;
    }
    ProtocolMetric metric;
    metric.name = body.substr(0, eq);
    metric.value = body.substr(eq + 1);
    metric.line = line;
    record->metrics.push_back(std::move(metric));
    return;
  }
  if (text.compare(0, 12, kNote) == 0) {
    const std::string body = text.substr(12);
    const std::size_t text_key = body.find(" text=");
    if (body.compare(0, 6, "basis=") != 0 || text_key == std::string::npos) {
      add_error(record, line, "SYSLAB_NOTE requires basis=CLASS text=...");
      return;
    }
    const std::string basis = body.substr(6, text_key - 6);
    if (!valid_basis(basis)) {
      add_error(record, line, "SYSLAB_NOTE basis is not recognized");
      return;
    }
    ProtocolNote note;
    note.basis = basis;
    note.text = body.substr(text_key + 6);
    note.line = line;
    record->notes.push_back(std::move(note));
    return;
  }
  if (text.compare(0, 12, kStep) == 0) {
    const std::string step = text.substr(12);
    if (step.empty()) {
      add_error(record, line, "SYSLAB_STEP is empty");
      return;
    }
    record->steps.push_back(step);
    return;
  }
  add_error(record, line, "unrecognized SYSLAB_ line");
}

}  // namespace

ProtocolRecord parse_protocol(const std::string& text) {
  ProtocolRecord record;
  std::size_t cursor = 0;
  int line = 1;
  while (cursor < text.size()) {
    const std::size_t end = text.find('\n', cursor);
    std::string current = text.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
    if (!current.empty() && current.back() == '\r') {
      current.pop_back();
    }
    parse_line(&record, line, current);
    if (end == std::string::npos) {
      break;
    }
    cursor = end + 1;
    ++line;
  }
  return record;
}

}  // namespace syslab
