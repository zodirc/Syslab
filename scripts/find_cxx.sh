#!/bin/sh
# 打印可用于本仓库的 C++ 编译器路径。GCC 8 不能作为 C++20 编译器。
if command -v clang++ >/dev/null 2>&1; then
  command -v clang++
  exit 0
fi
echo "未找到 clang++。" >&2
echo "SysLab 需要能使用 -std=c++20 的 Clang。本机若只有 GCC 8，不能构建。" >&2
exit 1
