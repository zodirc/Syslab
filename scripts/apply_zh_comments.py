#!/usr/bin/env python3
"""Insert the Chinese line under each English documentation comment."""

import pathlib
import sys

from zh_data import LINES
from zh_data2 import LINES2

ROOT = pathlib.Path(__file__).resolve().parents[1]
TAGS = ("@brief", "@param", "@return", "@throws", "@tparam")
EXTRA = {
    "Prints the pass marker and returns 0.": "打印通过标记并返回 0。",
    "Process status 0.": "进程状态 0。",
    "Prints a failure marker.": "打印失败标记。",
    "Short token without spaces.": "不含空格的短记号。",
    "Process status 1.": "进程状态 1。",
    "Prints an unsupported marker and returns 0 so the runner can classify it.": "打印不支持标记并返回 0，以便运行器把它归类。",
    "Prints one integer metric.": "打印一个整数度量。",
    "Metric token.": "度量记号。",
    "Measured value.": "测得的值。",
    "Prints a note tagged with its authority class.": "打印带依据类别的说明。",
    "LANG, ABI, COMPILER, OS, ISA, MICROARCH, or OBSERVED.": "LANG、ABI、COMPILER、OS、ISA、MICROARCH 或 OBSERVED。",
    "Note text without newlines.": "不含换行的说明文本。",
}


def load_map():
    english = (pathlib.Path("/tmp/en.txt")).read_text(encoding="utf-8").splitlines()
    chinese = LINES + LINES2
    if len(english) != len(chinese):
        raise SystemExit("translation count {0} != english {1}".format(len(chinese), len(english)))
    mapping = dict(EXTRA)
    for source, target in zip(english, chinese):
        mapping[source] = target
    return mapping


def has_cjk(text):
    return any("\u4e00" <= char <= "\u9fff" for char in text)


def star_body(line):
    stripped = line.strip()
    if stripped.startswith("/**"):
        stripped = stripped[3:].strip()
    if stripped.endswith("*/"):
        stripped = stripped[:-2].strip()
    if stripped.startswith("*"):
        stripped = stripped[1:].strip()
    return stripped


def description(body):
    if body.startswith("@param ") or body.startswith("@tparam "):
        parts = body.split(" ", 2)
        return parts[2] if len(parts) > 2 else ""
    if body.startswith(TAGS):
        return body.split(" ", 1)[1] if " " in body else ""
    return body


def star_line(sample, chinese):
    newline = "\n" if sample.endswith("\n") else ""
    star = sample.find("*")
    prefix = sample[: star + 1]
    return prefix + " " + chinese + newline


def annotate(block, mapping, missing):
    if any(has_cjk(line) for line in block):
        return block
    if len(block) == 1:
        return block
    result = [block[0]]
    index = 1
    end = len(block) - 1
    while index < end:
        body = star_body(block[index])
        if body == "":
            result.append(block[index])
            index += 1
            continue
        head = body.split(" ", 1)[0]
        if head in TAGS:
            result.append(block[index])
            key = description(body)
            if key not in mapping:
                missing.append(key)
            else:
                result.append(star_line(block[index], mapping[key]))
            index += 1
            continue
        prose_raw = []
        prose = []
        while index < end:
            body = star_body(block[index])
            if body == "" or body.split(" ", 1)[0] in TAGS:
                break
            prose_raw.append(block[index])
            prose.append(body)
            index += 1
        result.extend(prose_raw)
        key = " ".join(prose)
        if key not in mapping:
            missing.append(key)
        else:
            result.append(star_line(prose_raw[-1], mapping[key]))
    result.append(block[-1])
    return result


def transform(text, mapping, missing):
    lines = text.splitlines(keepends=True)
    output = []
    index = 0
    while index < len(lines):
        if not lines[index].strip().startswith("/**"):
            output.append(lines[index])
            index += 1
            continue
        block = [lines[index]]
        index += 1
        if "*/" not in block[0]:
            while index < len(lines):
                block.append(lines[index])
                index += 1
                if "*/" in block[-1]:
                    break
        output.extend(annotate(block, mapping, missing))
    return "".join(output)


def main():
    mapping = load_map()
    missing = []
    paths = list((ROOT / "include").rglob("*.hpp"))
    paths.append(ROOT / "experiments" / "common" / "check.hpp")
    changed = 0
    for path in paths:
        original = path.read_text(encoding="utf-8")
        updated = transform(original, mapping, missing)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed += 1
    if missing:
        unique = []
        for item in missing:
            if item not in unique:
                unique.append(item)
        print("missing", len(unique), file=sys.stderr)
        for item in unique:
            print(item, file=sys.stderr)
        return 1
    print("updated", changed, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
