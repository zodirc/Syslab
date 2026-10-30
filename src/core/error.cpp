#include "syslab/core/error.hpp"

namespace syslab {

const char* to_string(Error::Code code) noexcept {
  switch (code) {
    case Error::Code::InvalidArgument:
      return "invalid_argument";
    case Error::Code::NotFound:
      return "not_found";
    case Error::Code::Io:
      return "io";
    case Error::Code::Parse:
      return "parse";
    case Error::Code::Validation:
      return "validation";
    case Error::Code::Build:
      return "build";
    case Error::Code::Execution:
      return "execution";
    case Error::Code::Unsupported:
      return "unsupported";
    case Error::Code::Timeout:
      return "timeout";
    case Error::Code::ResourceLimit:
      return "resource_limit";
    case Error::Code::State:
      return "state";
    case Error::Code::Corruption:
      return "corruption";
    case Error::Code::Logic:
      return "logic";
  }
  return "logic";
}

}  // namespace syslab
