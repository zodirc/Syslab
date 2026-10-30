#!/usr/bin/env python3
"""Create a minimal experiment directory that the runner can build."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ID_RE = re.compile(r"^[A-Z]+-[0-9]{3}$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def main() -> int:
    if len(sys.argv) != 5:
        print("用法: python3 scripts/new_experiment.py ID NAME MODULE DESC", file=sys.stderr)
        print("示例: python3 scripts/new_experiment.py LAB-001 my-check lab.custom '检查返回值'", file=sys.stderr)
        return 1
    identifier, name, module, description = sys.argv[1:]
    if not ID_RE.match(identifier):
        print("ID 必须形如 LAB-001", file=sys.stderr)
        return 1
    if not TOKEN_RE.match(name):
        print("NAME 只能包含字母、数字、点、下划线和连字符", file=sys.stderr)
        return 1
    if not description or "\n" in description or '"' in description:
        print("DESC 需要是一行，并且不要包含英文双引号", file=sys.stderr)
        return 1
    target = "exp_" + identifier.lower().replace("-", "_")
    directory = ROOT / "experiments" / "custom" / name
    if directory.exists():
        print("目录已存在: " + str(directory), file=sys.stderr)
        return 1
    directory.mkdir(parents=True)
    (directory / "expected").mkdir()
    (directory / "experiment.yaml").write_text(
        """schema_version: 1
experiment:
  id: {id}
  name: {name}
  module: {module}
  description: "{description}"
  risk: normal
  tags:
    - custom
platform:
  os: linux
  architectures:
    - x86_64
    - aarch64
build:
  target: {target}
  standard: c++20
  variants:
    - name: debug
      build_type: Debug
run:
  executable: {target}
  timeout_ms: 10000
  repetitions: 1
  arguments: []
observe:
  - stdout
  - stderr
  - exit_status
verify:
  checks:
    - name: exit
      type: exit_code
      expected: "0"
    - name: marker
      type: result_marker
      expected: pass
artifacts:
  preserve:
    - executable
    - build_log
knowledge:
  concepts:
    - custom
  basis:
    - OBSERVED
""".format(id=identifier, name=name, module=module, description=description, target=target),
        encoding="utf-8")
    (directory / "main.cpp").write_text(
        """#include "experiments/common/check.hpp"

int main() {
  // 在这里写可以判定对错的检查。不要把猜测写成通过。
  syslab_note("OBSERVED", "replace this note with what the program actually checked");
  return syslab_pass();
}
""",
        encoding="utf-8")
    (directory / "README.md").write_text(
        """# {id} {name}

## 实验目的

{description}

## 程序实际检查的内容

把 main.cpp 里的判定写在这里。

## 不由本实验断言的内容

写明哪些现象只是观察，不能当作通过条件。
""".format(id=identifier, name=name, description=description),
        encoding="utf-8")
    (directory / "expected" / "behavior.md").write_text(
        """# 预期行为

## 通过条件

进程退出码为 0，并且标准输出包含 `SYSLAB_RESULT status=pass`。

## 边界

主机做不到时输出 `SYSLAB_RESULT status=unsupported`，不要编造数据。
""",
        encoding="utf-8")
    print(directory)
    print("下一步: make run ID={0}".format(identifier))
    return 0


if __name__ == "__main__":
    sys.exit(main())
