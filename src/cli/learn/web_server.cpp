#include "../learn.hpp"
#include "internal.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/experiment/model.hpp"
#include "syslab/observation/protocol.hpp"

#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace syslab {

bool send_all(int fd, const char* data, std::size_t size) {
  std::size_t sent = 0;
  while (sent < size) {
    const ssize_t count = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

std::string request_path(const std::string& request) {
  if (request.compare(0, 4, "GET ") != 0 && request.compare(0, 5, "HEAD ") != 0 && request.compare(0, 5, "POST ") != 0) {
    return {};
  }
  const std::size_t start = request.find(' ');
  if (start == std::string::npos) {
    return {};
  }
  const std::size_t path_begin = start + 1;
  const std::size_t path_end = request.find(' ', path_begin);
  if (path_end == std::string::npos) {
    return {};
  }
  std::string path = request.substr(path_begin, path_end - path_begin);
  const std::size_t query = path.find('?');
  if (query != std::string::npos) {
    path.resize(query);
  }
  return path;
}

void reply_redirect(int client, const std::string& location) {
  const std::string header = "HTTP/1.1 303 See Other\r\nLocation: " + location + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  send_all(client, header.data(), header.size());
}

void reply_html(int client, int status, const std::string& body, bool head_only) {
  const char* phrase = "OK";
  if (status == 400) {
    phrase = "Bad Request";
  } else if (status == 403) {
    phrase = "Forbidden";
  } else if (status == 404) {
    phrase = "Not Found";
  } else if (status == 409) {
    phrase = "Conflict";
  } else if (status == 500) {
    phrase = "Internal Server Error";
  } else if (status != 200) {
    phrase = "Error";
  }
  std::ostringstream header;
  header << "HTTP/1.1 " << status << " " << phrase << "\r\n"
         << "Content-Type: text/html; charset=utf-8\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n\r\n";
  const std::string head = header.str();
  if (!send_all(client, head.data(), head.size())) {
    return;
  }
  if (!head_only && !body.empty()) {
    send_all(client, body.data(), body.size());
  }
}

std::string header_value(const std::string& request, const std::string& name) {
  const std::size_t header_end = request.find("\r\n\r\n");
  const std::string headers = request.substr(0, header_end == std::string::npos ? request.size() : header_end);
  std::string lowered = headers;
  for (char& character : lowered) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  std::string key = "\r\n" + name + ":";
  for (char& character : key) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  const std::size_t at = lowered.find(key);
  if (at == std::string::npos) {
    return {};
  }
  std::size_t value = at + key.size();
  while (value < headers.size() && headers[value] == ' ') {
    ++value;
  }
  const std::size_t line_end = headers.find("\r\n", value);
  return headers.substr(value, line_end == std::string::npos ? std::string::npos : line_end - value);
}

bool host_allowed(const std::string& host, int port) {
  const std::string suffix = ":" + std::to_string(port);
  return host == "127.0.0.1" + suffix || host == "localhost" + suffix;
}

bool origin_allowed(const std::string& origin, int port) {
  if (origin.empty()) {
    return true;
  }
  const std::string text = std::to_string(port);
  return origin == "http://127.0.0.1:" + text || origin == "http://localhost:" + text;
}

std::string form_value(const std::string& body, const std::string& name) {
  const std::string key = name + "=";
  std::size_t at = 0;
  while (at < body.size()) {
    if ((at == 0 || body[at - 1] == '&') && body.compare(at, key.size(), key) == 0) {
      const std::size_t start = at + key.size();
      const std::size_t end = body.find('&', start);
      return body.substr(start, end == std::string::npos ? std::string::npos : end - start);
    }
    const std::size_t next = body.find('&', at);
    if (next == std::string::npos) {
      break;
    }
    at = next + 1;
  }
  return {};
}

std::string random_token() {
  std::ifstream input("/dev/urandom", std::ios::in | std::ios::binary);
  unsigned char bytes[16];
  input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
    throw Error(Error::Code::Io, "无法生成网页运行口令");
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (unsigned char byte : bytes) {
    out << std::setw(2) << static_cast<int>(byte);
  }
  return out.str();
}

void set_socket_timeouts(int fd) {
  timeval timeout {};
  timeout.tv_sec = 5;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

bool read_request(int client, std::string* request) {
  char buffer[1024];
  while (request->find("\r\n\r\n") == std::string::npos && request->size() < 65536) {
    const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    request->append(buffer, static_cast<std::size_t>(count));
  }
  const std::size_t header_end = request->find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return false;
  }
  const std::string length_text = header_value(*request, "Content-Length");
  std::size_t length = 0;
  if (!length_text.empty()) {
    try {
      length = static_cast<std::size_t>(std::stoul(length_text));
    } catch (const std::exception&) {
      return false;
    }
  }
  if (length > 65536) {
    return false;
  }
  while (request->size() < header_end + 4 + length) {
    const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    request->append(buffer, static_cast<std::size_t>(count));
  }
  return true;
}

std::string request_body(const std::string& request) {
  const std::size_t header_end = request.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return {};
  }
  return request.substr(header_end + 4);
}

int command_web(const std::string& root, int port, const SessionConfig& config, Registry& registry) {
  if (port < 1 || port > 65535) {
    throw Error(Error::Code::InvalidArgument, "端口应为 1 到 65535 的整数");
  }
  const std::vector<Lesson> lessons = load_course(root, registry);
  const std::map<std::string, std::string> figures = load_figures(root);
  const LessonCopy copy = load_briefs(root);
  const std::string run_token = random_token();
  write_course_page(root, lessons, registry, figures, copy, "", {}, run_token);
  const std::string page_path = join_path(root, "artifacts/lessons/index.html");

  const int server = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    throw Error(Error::Code::Io, "无法创建网页套接字");
  }
  const int reuse = 1;
  ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(server);
    throw Error(Error::Code::Io, "端口 " + std::to_string(port) + " 无法绑定。换一个：make web PORT=9528");
  }
  if (::listen(server, 16) != 0) {
    ::close(server);
    throw Error(Error::Code::Io, "网页服务无法开始监听");
  }
  std::cout << "http://127.0.0.1:" << port << "/\n";
  std::cout << "页面中的「执行」在当前主机编译并运行该节。Ctrl+C 停止服务。\n";
  std::cout.flush();

  std::mutex gate;
  std::condition_variable idle;
  std::thread worker;
  bool busy = false;
  const auto reap_worker = [&]() {
    std::thread finished;
    {
      std::lock_guard<std::mutex> lock(gate);
      if (!busy && worker.joinable()) {
        finished = std::move(worker);
      }
    }
    if (finished.joinable()) {
      finished.join();
    }
  };
  const auto stop_worker = [&]() {
    std::unique_lock<std::mutex> lock(gate);
    idle.wait(lock, [&]() { return !busy; });
    std::thread finished = std::move(worker);
    lock.unlock();
    if (finished.joinable()) {
      finished.join();
    }
  };

  while (true) {
    reap_worker();
    const int client = ::accept(server, nullptr, nullptr);
    if (client < 0) {
      if (errno == EINTR) {
        continue;
      }
      stop_worker();
      ::close(server);
      throw Error(Error::Code::Io, "接受网页连接失败");
    }
    set_socket_timeouts(client);
    std::string request;
    const bool complete = read_request(client, &request);
    const bool head_only = request.compare(0, 5, "HEAD ") == 0;
    const bool post = request.compare(0, 5, "POST ") == 0;
    const std::string path = request_path(request);
    const std::string host = header_value(request, "Host");
    bool handed = false;
    if (!complete) {
      reply_html(client, 400, "<!DOCTYPE html><meta charset=\"utf-8\"><p>请求不完整。</p>\n", false);
    } else if (!host_allowed(host, port)) {
      reply_html(client, 403, "<!DOCTYPE html><meta charset=\"utf-8\"><p>只接受本机页面。</p>\n", false);
    } else if (post && path.compare(0, 5, "/run/") == 0 && path.find('/', 5) == std::string::npos) {
      if (!origin_allowed(header_value(request, "Origin"), port) ||
          form_value(request_body(request), "token") != run_token) {
        reply_html(client, 403, "<!DOCTYPE html><meta charset=\"utf-8\"><p>运行口令不正确。</p>\n", false);
      } else {
        std::size_t index = lessons.size();
        const std::string id = path.substr(5);
        for (std::size_t cursor = 0; cursor < lessons.size(); ++cursor) {
          if (lessons[cursor].id == id) {
            index = cursor;
            break;
          }
        }
        if (index == lessons.size()) {
          reply_html(client, 404, "<!DOCTYPE html><meta charset=\"utf-8\"><p>没有这一节。</p>\n", false);
        } else {
          std::lock_guard<std::mutex> lock(gate);
          if (busy) {
            reply_html(client, 409, "<!DOCTYPE html><meta charset=\"utf-8\"><p>已有一节正在运行。</p>\n", false);
          } else {
            if (worker.joinable()) {
              worker.join();
            }
            busy = true;
            handed = true;
            const int owned = client;
            worker = std::thread([&, owned, index]() {
              try {
                const Lesson& lesson = lessons[index];
                const Manifest& manifest = registry.get(lesson.id);
                std::vector<RunView> runs;
                SessionConfig run_config = config;
                run_config.variant.clear();
                const SessionOutput first = execute_session(manifest, run_config);
                runs.push_back(RunView{lesson.both ? "未优化" : "", first.run_directory, first.state});
                if (lesson.both) {
                  run_config.variant = "optimized";
                  const SessionOutput second = execute_session(manifest, run_config);
                  runs.push_back(RunView{"优化后", second.run_directory, second.state});
                }
                write_course_page(root, lessons, registry, figures, copy, lesson.id, runs, run_token);
                reply_redirect(owned, "/#" + lesson.id);
              } catch (const std::exception& error) {
                reply_html(owned, 500,
                           "<!DOCTYPE html><meta charset=\"utf-8\"><p>运行没有完成：" + html_escape(error.what()) +
                               "</p>\n",
                           false);
              }
              {
                std::lock_guard<std::mutex> done(gate);
                busy = false;
                idle.notify_all();
              }
              ::close(owned);
            });
          }
        }
      }
    } else if (path == "/" || path == "/index.html") {
      reply_html(client, 200, read_file(page_path), head_only);
    } else {
      reply_html(client, 404, "<!DOCTYPE html><meta charset=\"utf-8\"><p>这个地址没有页面。</p>\n", head_only);
    }
    if (!handed) {
      ::close(client);
    }
  }
  return 0;
}

}  // namespace syslab
