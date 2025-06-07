/**
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22

 * @file common.hpp
 * @brief 公共定义和工具函数
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <mcap/mcap.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace openbag {

/**
 * @brief 压缩算法类型
 */
enum class CompressionType {
  NONE, ///< 不压缩
  LZ4,  ///< LZ4压缩
  ZSTD, ///< ZSTD压缩
};

/**
 * @brief 存储格式类型
 */
enum class StorageFormat {
  MCAP,    ///< MCAP格式
  PROTOBUF ///< 原生Protobuf格式
};

/**
 * @brief 话题信息结构
 */
struct TopicInfo {
  std::string topic_name;            ///< 话题名称
  std::string proto_type;            ///< Protobuf类型
  std::string proto_file;            ///< Protobuf文件
  mcap::SchemaId schema_id;          ///< 模式ID
  mcap::ChannelId channel_id;        ///< 通道ID
  std::string encoding = "protobuf"; ///< 编码格式，默认为protobuf
};

/**
 * @brief 统一消息结构定义，支持MCAP和Protobuf格式
 */
struct Message {
  std::string topic;                 ///< 消息所属的主题
  std::string data;                  ///< 消息的原始数据(支持二进制)
  uint64_t timestamp;                ///< 时间戳(纳秒)
  uint64_t sequence_number;          ///< 消息的序列号
  std::string schema_name;           ///< 模式名称(用于动态解析)
  std::string encoding = "protobuf"; ///< 编码格式

  /**
   * @brief 构造函数
   */
  Message() : timestamp(0), sequence_number(0) {}
};

/**
 * @brief 文件信息结构
 */
struct FileInfo {
  bool is_open = false;                       ///< 是否打开
  uint64_t file_size = 0;                     ///< 文件大小
  std::string prefix = "";                    ///< 文件名前缀
  std::string extension = "";                 ///< 文件扩展名
  std::string filename = "";                  ///< 文件名
  std::string output_path = "";               ///< 输出路径
  StorageFormat format = StorageFormat::MCAP; ///< 存储格式

  FileInfo() = default;
  FileInfo(const FileInfo &other)
      : is_open(other.is_open), file_size(other.file_size),
        prefix(other.prefix), extension(other.extension),
        filename(other.filename), output_path(other.output_path),
        format(other.format) {}
};

using MessagePtr = std::shared_ptr<Message>;

/**
 * @brief 获取当前系统时间的纳秒级时间戳
 * @return uint64_t 纳秒级时间戳
 */
inline uint64_t GetCurrentTimestampNs() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

/**
 * @brief 获取当前系统时间的微秒级时间戳
 * @return int64_t 微秒级时间戳
 */
inline int64_t GetCurrentTimestampUs() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration)
      .count();
}

/**
 * @brief 将纳秒时间戳转换为可读的时间字符串
 * @param timestamp_ns 纳秒级时间戳
 * @return std::string 格式化的时间字符串
 */
inline std::string TimestampNsToString(uint64_t timestamp_ns) {
  auto seconds = static_cast<time_t>(timestamp_ns / 1000000000ULL);
  auto nanoseconds = timestamp_ns % 1000000000ULL;

  std::tm timeinfo;
  localtime_r(&seconds, &timeinfo);

  std::ostringstream ss;
  ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "."
     << std::setfill('0') << std::setw(9) << nanoseconds;

  return ss.str();
}

/**
 * @brief 将时间戳转换为可读的时间字符串
 * @param timestamp 微秒级时间戳
 * @return std::string 格式化的时间字符串
 */
inline std::string TimestampToString(int64_t timestamp) {
  auto microseconds = static_cast<time_t>(timestamp / 1000000);
  auto remainder = timestamp % 1000000;

  std::tm timeinfo;
  localtime_r(&microseconds, &timeinfo);

  std::ostringstream ss;
  ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "."
     << std::setfill('0') << std::setw(6) << remainder;

  return ss.str();
}

/**
 * @brief 获取带格式的当前时间字符串
 * @param format 时间格式，默认为 "%Y-%m-%d %H:%M:%S"
 * @return 格式化的时间字符串
 */
inline std::string
GetTimeString(const std::string &format = "%Y-%m-%d %H:%M:%S") {
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  std::tm timeinfo;
  localtime_r(&now_time_t, &timeinfo);
  std::stringstream ss;
  ss << std::put_time(&timeinfo, format.c_str());
  return ss.str();
}

/**
 * @brief 生成唯一的文件名，基于当前时间
 * @param prefix 文件名前缀
 * @param extension 文件扩展名（不带点）
 * @return std::string 唯一的文件名
 */
inline std::string GenerateUniqueFilename(const std::string &prefix,
                                          const std::string &extension) {
  std::string time_str = GetTimeString("%Y_%m_%d-%H_%M_%S");

  std::ostringstream ss;
  ss << prefix << "_" << time_str << "." << extension;

  return ss.str();
}

/**
 * @brief 判断字符串是否以指定后缀结尾
 *
 * @param str 待判断的字符串
 * @param suffix 指定的后缀
 * @return bool 如果字符串以指定后缀结尾，返回true，否则返回false
 */
inline bool EndsWith(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length())
    return false;
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) ==
         0;
}

inline std::string ConvertToString(const std::byte *data, size_t size) {
  // 创建一个字符串，大小为size
  std::string result(size, '\0'); // 初始化为'\0'字符

  // 将std::byte转换为char并填充到字符串中
  for (size_t i = 0; i < size; ++i) {
    result[i] = static_cast<char>(data[i]); // 将std::byte转换为char
  }

  return result;
}
} // namespace openbag
