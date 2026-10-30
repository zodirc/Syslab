#include "syslab/core/time.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace syslab {

std::string utc_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc {};
  gmtime_r(&now, &utc);
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::uint64_t steady_nanoseconds() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::string make_id(const std::string& prefix) {
  const std::time_t now = std::time(nullptr);
  std::tm utc {};
  gmtime_r(&now, &utc);
  std::ostringstream stamp;
  stamp << std::put_time(&utc, "%Y%m%dT%H%M%S");
  std::random_device device;
  std::mt19937 generator(device());
  std::uniform_int_distribution<int> hex_digit(0, 15);
  std::string suffix(8, '0');
  const char* digits = "0123456789abcdef";
  for (char& character : suffix) {
    character = digits[hex_digit(generator)];
  }
  return prefix + "-" + stamp.str() + "-" + suffix;
}

}  // namespace syslab
