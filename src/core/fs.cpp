#include "syslab/core/fs.hpp"

#include "syslab/core/error.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

namespace syslab {
namespace {

void throw_errno(const std::string& action, const std::string& path) {
  throw Error(Error::Code::Io, action + " failed for " + path + ": " + std::strerror(errno));
}

bool is_skipped_directory(const std::string& name) {
  return name == "artifacts" || name == "build" || name == ".git";
}

void walk(const std::string& directory, const std::string& filename,
          std::vector<std::string>* matches) {
  DIR* handle = opendir(directory.c_str());
  if (handle == nullptr) {
    throw_errno("opendir", directory);
  }
  while (dirent* entry = readdir(handle)) {
    const std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    const std::string path = join_path(directory, name);
    struct stat status {};
    if (lstat(path.c_str(), &status) != 0) {
      closedir(handle);
      throw_errno("lstat", path);
    }
    if (S_ISDIR(status.st_mode)) {
      if (!is_skipped_directory(name)) {
        walk(path, filename, matches);
      }
    } else if (S_ISREG(status.st_mode) && name == filename) {
      matches->push_back(path);
    }
  }
  closedir(handle);
}

}  // namespace

std::string read_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw Error(Error::Code::Io, "unable to read " + path);
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (input.bad()) {
    throw Error(Error::Code::Io, "unable to finish reading " + path);
  }
  return buffer.str();
}

void write_file(const std::string& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw Error(Error::Code::Io, "unable to write " + path);
  }
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!output) {
    throw Error(Error::Code::Io, "unable to finish writing " + path);
  }
}

void create_directories(const std::string& path) {
  if (path.empty() || path == "/") {
    return;
  }
  std::string current;
  std::size_t index = 0;
  if (path[0] == '/') {
    current = "/";
    index = 1;
  }
  while (index <= path.size()) {
    const std::size_t slash = path.find('/', index);
    const std::string part = path.substr(index, slash == std::string::npos ? std::string::npos : slash - index);
    if (!part.empty() && part != ".") {
      if (current.empty() || current == "/") {
        current += part;
      } else {
        current = join_path(current, part);
      }
      if (!path_exists(current)) {
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
          throw_errno("mkdir", current);
        }
      } else if (!is_directory(current)) {
        throw Error(Error::Code::Io, current + " exists and is not a directory");
      }
    }
    if (slash == std::string::npos) {
      break;
    }
    index = slash + 1;
  }
}

bool path_exists(const std::string& path) {
  struct stat status {};
  return stat(path.c_str(), &status) == 0;
}

bool is_directory(const std::string& path) {
  struct stat status {};
  return stat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

std::vector<std::string> find_files_named(const std::string& root, const std::string& filename) {
  std::vector<std::string> matches;
  if (!is_directory(root)) {
    throw Error(Error::Code::Io, "experiment root is not a directory: " + root);
  }
  walk(root, filename, &matches);
  return matches;
}

std::string join_path(const std::string& directory, const std::string& relative) {
  if (directory.empty() || directory == ".") {
    return relative;
  }
  if (relative.empty()) {
    return directory;
  }
  if (directory.back() == '/') {
    return directory + relative;
  }
  return directory + "/" + relative;
}

std::string parent_path(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return {};
  }
  if (slash == 0) {
    return "/";
  }
  return path.substr(0, slash);
}

std::string filename_of(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return path;
  }
  return path.substr(slash + 1);
}

}  // namespace syslab
