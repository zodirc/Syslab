#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace syslab {

/**
 * @brief Computes the SHA-256 digest of a byte sequence.
 * 计算字节序列的 SHA-256。
 * @param data Pointer to the first byte. May be null only when size is zero.
 * 首字节指针。仅当长度为 0 时可以为空。
 * @param size Number of bytes.
 * 字节数。
 * @return Thirty-two-byte digest.
 * 32 字节摘要。
 */
std::array<std::uint8_t, 32> sha256(const void* data, std::size_t size);

/**
 * @brief Computes the SHA-256 digest of a string.
 * 计算字符串的 SHA-256。
 * @param data Input bytes.
 * 输入字节。
 * @return Thirty-two-byte digest.
 * 32 字节摘要。
 */
std::array<std::uint8_t, 32> sha256(const std::string& data);

/**
 * @brief Computes the SHA-256 digest of a file.
 * 计算文件的 SHA-256。
 * @param path Filesystem path.
 * 文件系统路径。
 * @return Thirty-two-byte digest.
 * 32 字节摘要。
 * @throws Error Io when the file cannot be read.
 * 文件无法读取时抛出 Error Io。
 */
std::array<std::uint8_t, 32> sha256_file(const std::string& path);

/**
 * @brief Encodes a digest as lowercase hexadecimal.
 * 把摘要编码为小写十六进制。
 * @param digest SHA-256 digest.
 * SHA-256 摘要。
 * @return Sixty-four hexadecimal characters.
 * 64 个十六进制字符。
 */
std::string to_hex(const std::array<std::uint8_t, 32>& digest);

}  // namespace syslab
