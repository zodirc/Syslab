#include "experiments/common/check.hpp"

#include <string>
#include <vector>

// 现象：预算 12 字节。写入 hello、world、!! 之后再写入 xxxx，hello 被丢掉，world、!!、xxxx 还在。
// 原理：追加后若总字节数超过预算，就从最旧的整条消息开始丢，直到不超过预算或只剩最新一条。
namespace {

std::size_t total_bytes(const std::vector<std::string>& messages) {
  std::size_t sum = 0;
  for (const std::string& message : messages) {
    sum += message.size();
  }
  return sum;
}

void append_message(std::vector<std::string>* messages, const std::string& text, std::size_t budget) {
  messages->push_back(text);
  while (total_bytes(*messages) > budget && messages->size() > 1) {
    messages->erase(messages->begin());
  }
}

bool contains(const std::vector<std::string>& messages, const char* text) {
  for (const std::string& message : messages) {
    if (message == text) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  std::vector<std::string> messages;
  constexpr std::size_t kBudget = 12;
  append_message(&messages, "hello", kBudget);
  append_message(&messages, "world", kBudget);
  append_message(&messages, "!!", kBudget);
  append_message(&messages, "xxxx", kBudget);
  if (contains(messages, "hello") || !contains(messages, "world") || !contains(messages, "!!") ||
      !contains(messages, "xxxx") || messages.back() != "xxxx" || total_bytes(messages) > kBudget) {
    syslab_step("超过预算后保留下来的消息不是 world、!!、xxxx。");
    return syslab_fail("budget");
  }
  syslab_metric("bytes", static_cast<long long>(total_bytes(messages)));
  syslab_note("LANG", "a byte budget drops the oldest whole messages and keeps the newest");
  syslab_step("预算 12 字节。hello 已丢。剩下 world、!!、xxxx，最新一条是 xxxx。");
  return syslab_pass();
}
