#pragma once

#include "syslab/experiment/registry.hpp"
#include "syslab/experiment/session.hpp"

#include <string>

namespace syslab {

/**
 * @brief Prints the course or teaches one lesson.
 * 打印课程表，或上某一课。
 *
 * An empty argument lists the course. A decimal argument selects a lesson
 * number. "next" continues from the progress file. An experiment id selects
 * that lesson.
 * 空参数列出课程。十进制参数选择课次。"next" 从进度文件继续。实验编号选择对应的课。
 *
 * @param root Repository root.
 * 仓库根目录。
 * @param argument Lesson selector. May be empty.
 * 课次选择。可以为空。
 * @param config Paths and toolchain used to run a lesson.
 * 上课时使用的路径和工具链。
 * @param registry Loaded experiment manifests.
 * 已加载的实验清单。
 * @return Process status: 0 passed, 2 failed, 3 unsupported, 4 interrupted, 1 usage.
 * 进程状态：0 通过，2 失败，3 不支持，4 中断，1 用法错误。
 */
int command_learn(const std::string& root, const std::string& argument, const SessionConfig& config,
                  Registry& registry);

/**
 * @brief Writes the course page and serves it on the loopback interface.
 * 写出课程页，并在本机回环地址上提供该页。
 *
 * The process keeps running until it is interrupted. Each request reads the
 * page from disk, so a later lesson run is visible after a browser refresh.
 * 进程一直运行到被中断。每次请求都从磁盘读取页面，因此之后上的课刷新浏览器就能看到。
 *
 * @param root Repository root.
 * 仓库根目录。
 * @param port TCP port on 127.0.0.1. Must be from 1 to 65535.
 * 127.0.0.1 上的 TCP 端口。必须在 1 到 65535 之间。
 * @param config Paths and toolchain used when the page runs a section.
 * 页面上的运行按钮使用的路径和工具链。
 * @param registry Loaded experiment manifests.
 * 已加载的实验清单。
 * @return 0 when the server stops without an error. Bind failures throw.
 * 服务器无错误停下时返回 0。绑定失败时抛出异常。
 */
int command_web(const std::string& root, int port, const SessionConfig& config, Registry& registry);

}  // namespace syslab
