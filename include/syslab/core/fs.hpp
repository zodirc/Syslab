#pragma once

#include <string>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief Reads an entire file as bytes interpreted as text.
 * 把整个文件按文本读入。
 * @param path Filesystem path.
 * 文件系统路径。
 * @return File contents.
 * 文件内容。
 * @throws Error Io when the file cannot be opened or read.
 * 无法打开或读完文件时抛出 Error Io。
 */
std::string read_file(const std::string& path);

/**
 * @brief Creates or replaces a file with the given contents.
 * 用给定内容创建或覆盖文件。
 * @param path Destination path. Parent directories must already exist.
 * 目标路径。父目录必须已经存在。
 * @param contents Bytes written to the file.
 * 写入文件的字节。
 * @throws Error Io when the file cannot be written completely.
 * 无法完整写入时抛出 Error Io。
 */
void write_file(const std::string& path, const std::string& contents);

/**
 * @brief Creates a directory and any missing parents.
 * 创建目录以及缺失的父目录。
 * @param path Directory path.
 * 目录路径。
 * @throws Error Io when a component cannot be created.
 * 某一级目录无法创建时抛出 Error Io。
 */
void create_directories(const std::string& path);

/**
 * @brief Reports whether a path exists.
 * 报告路径是否存在。
 * @param path Filesystem path.
 * 文件系统路径。
 * @return True when stat succeeds for the path.
 * 对该路径 stat 成功时为真。
 */
bool path_exists(const std::string& path);

/**
 * @brief Reports whether a path names a directory.
 * 报告路径是否为目录。
 * @param path Filesystem path.
 * 文件系统路径。
 * @return True when the path is a directory.
 * 路径是目录时为真。
 */
bool is_directory(const std::string& path);

/**
 * @brief Recursively finds files with an exact base name.
 * 按确切文件名递归查找。
 * @param root Directory that starts the walk.
 * 开始遍历的目录。
 * @param filename File name to match, without directories.
 * 要匹配的文件名，不含目录。
 * @return Matching paths. Directories named artifacts, build, and .git are skipped.
 * 匹配到的路径。名为 artifacts、build 和 .git 的目录会被跳过。
 * @throws Error Io when a directory cannot be read.
 * 目录无法读取时抛出 Error Io。
 */
std::vector<std::string> find_files_named(const std::string& root, const std::string& filename);

/**
 * @brief Joins a directory and a relative path with a single separator.
 * 用一个分隔符连接目录和相对路径。
 * @param directory Parent directory. May be empty.
 * 父目录。可以为空。
 * @param relative Relative component.
 * 相对路径部分。
 * @return Combined path.
 * 连接后的路径。
 */
std::string join_path(const std::string& directory, const std::string& relative);

/**
 * @brief Returns the parent directory of a path.
 * 返回路径的父目录。
 * @param path File or directory path.
 * 文件或目录路径。
 * @return Parent path, or empty when the path has no separator.
 * 父路径。路径里没有分隔符时为空。
 */
std::string parent_path(const std::string& path);

/**
 * @brief Returns the file name component of a path.
 * 返回路径中的文件名部分。
 * @param path File or directory path.
 * 文件或目录路径。
 * @return Substring after the last separator, or the original path.
 * 最后一个分隔符之后的子串；没有分隔符时返回原路径。
 */
std::string filename_of(const std::string& path);

/**
 * @brief Lists the names inside a directory.
 * 列出目录中的名字。
 *
 * The result omits "." and "..". It does not recurse.
 * 结果不含 "." 和 ".."，也不递归。
 *
 * @param path Directory path.
 * 目录路径。
 * @return Entry names in directory order.
 * 按目录顺序排列的条目名。
 * @throws Error Io when the directory cannot be read.
 * 目录无法读取时抛出 Error Io。
 */
std::vector<std::string> list_directory(const std::string& path);

/**
 * @brief Deletes a file or a directory tree.
 * 删除一个文件或一棵目录树。
 *
 * A missing path is not an error. Directory contents are removed before the
 * directory itself.
 * 路径不存在不是错误。先删除目录内容，再删除目录本身。
 *
 * @param path File or directory to remove.
 * 要删除的文件或目录。
 * @throws Error Io when an entry cannot be removed.
 * 某个条目无法删除时抛出 Error Io。
 */
void remove_tree(const std::string& path);

/**
 * @brief Appends bytes to a file with one O_APPEND write.
 * 用一次 O_APPEND 写把字节追加到文件。
 *
 * The file is created when it does not exist. The caller should keep one
 * record inside a single write so concurrent appends do not tear that record.
 * 文件不存在时会创建。调用方应把一条记录放进同一次写，避免并发追加把该记录写裂。
 *
 * @param path Destination path. Parent directories must already exist.
 * 目标路径。父目录必须已经存在。
 * @param contents Bytes to append.
 * 要追加的字节。
 * @throws Error Io when the file cannot be opened or the write is short.
 * 无法打开文件或写入不完整时抛出 Error Io。
 */
void append_file(const std::string& path, const std::string& contents);

}  // namespace syslab
