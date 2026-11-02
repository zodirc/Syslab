#include "experiments/common/check.hpp"

#include <string>
#include <vector>

// 现象：第一步失败时，完成列表是空的。第一步成功时，完成列表是 one、two。
// 原理：后一步只在前一步返回成功之后执行。
namespace {

bool step_one(bool succeed, std::vector<std::string>* done) {
  if (!succeed) {
    return false;
  }
  done->push_back("one");
  return true;
}

void step_two(std::vector<std::string>* done) {
  done->push_back("two");
}

void run(bool succeed, std::vector<std::string>* done) {
  done->clear();
  if (!step_one(succeed, done)) {
    return;
  }
  step_two(done);
}

}  // namespace

int main() {
  std::vector<std::string> failed;
  run(false, &failed);
  std::vector<std::string> passed;
  run(true, &passed);
  if (!failed.empty() || passed.size() != 2 || passed[0] != "one" || passed[1] != "two") {
    syslab_step("失败路径执行了第二步，或成功路径的顺序不是 one、two。");
    return syslab_fail("gate");
  }
  syslab_note("LANG", "a later step runs only after the previous step returns success");
  syslab_step("第一步失败时没有完成记录。第一步成功时完成记录是 one、two。");
  return syslab_pass();
}
