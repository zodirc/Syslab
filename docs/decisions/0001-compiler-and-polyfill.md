# 0001. 使用 Clang 12 和协程 polyfill

## 状态

已接受。

## 背景

开发主机的 GCC 是 8.5.0。它不识别 `-std=c++20`，只接受 `-std=c++2a`，且 libstdc++ 8 没有 `<coroutine>`、`<span>` 和 `<format>`。Clang 12 能以 C++20 编译，并链接这套 libstdc++。Clang 在缺少标准头时查找 `std::experimental::coroutine_traits`。

## 决定

CMake 要求编译器是 Clang 或 GCC，实际验证使用 Clang。标准库没有 `<coroutine>` 时，把 `third_party/polyfill/include` 放到系统头之前。polyfill 同时定义 `std::experimental` 和 `std` 中的协程类型。存在标准 `<coroutine>` 时不使用 polyfill。

核心代码不使用 `<format>` 和 `<span>`。

## 后果

GCC 8 不能构建本仓库。换用带完整 C++20 标准库的 GCC 或 Clang 后，应重新运行测试，并确认 polyfill 没有被错误包含。
