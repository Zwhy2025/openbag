/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file config.hpp
 * @brief 配置管理
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "openbag/common.hpp"

namespace openbag {

/**
 * @brief 存储配置
 */
struct StorageConfig
{
    int compression_level = 0;                                 ///< 压缩级别
    CompressionType compression_type = CompressionType::NONE;  ///< 压缩类型
    std::vector<std::string> proto_search_paths;               ///< proto搜索路径

    size_t write_batch_size = 1000;
    uint64_t max_file_size = 1024 * 1024 * 1024;
    uint64_t chunk_size = 1024;
    bool split_by_size = true;

    /**
     * @brief 构造函数，设置默认值
     */
    StorageConfig() {}
};

/**
 * @brief 录制配置
 */
struct RecorderConfig
{
    /** output */
    std::string output_path = "./openbag/";   ///< 输出路径
    std::string filename_prefix = "openbag";  ///< 文件名前缀
    std::string output_format = "mcap";       ///< 输出格式 (mcap 或 proto)

    /** record */
    std::vector<TopicInfo> topics;  ///< 订阅的话题列表

    void LoadConfig(const std::string& config_file) { YAML::Node config = YAML::LoadFile(config_file); }
};

/**
 * @brief 播放配置
 */
struct PlayerConfig
{
    std::string input_path;  ///< 输入文件路径
    bool loop_playback;      ///< 是否循环播放
    double playback_rate;    ///< 播放速率
    StorageConfig storage;   ///< 存储配置

    /**
     * @brief 构造函数，设置默认值
     */
    PlayerConfig() : loop_playback(false), playback_rate(1.0) {}
};

struct BufferConfig
{
    size_t buffer_size = 10000;
};

/**
 * @brief 配置管理类
 */
class ConfigManager
{
public:
    /**
     * @brief 默认构造函数
     */
    ConfigManager() = default;

    /**
     * @brief 从YAML文件加载录制配置
     * @param config_file 配置文件路径
     * @return 是否加载成功
     */
    bool LoadRecorderConfig(const std::string& config_file)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(config_file);

            // 解析输出路径
            if (config["output"] && config["output"]["output_path"])
            {
                m_recorderConfig.output_path = config["output"]["output_path"].as<std::string>();
            }

            // 解析文件名前缀
            if (config["output"] && config["output"]["filename_prefix"])
            {
                m_recorderConfig.filename_prefix = config["output"]["filename_prefix"].as<std::string>();
            }

            // 解析输出格式
            if (config["output"] && config["output"]["output_format"])
            {
                m_recorderConfig.output_format = config["output"]["output_format"].as<std::string>();
            }

            // 解析主题到消息类型的映射和主题到proto文件的映射
            if (config["topics"] && config["topics"].IsSequence())
            {
                m_recorderConfig.topics.clear();
                for (const auto& topic : config["topics"])
                {
                    if (topic["name"] && topic["type"] && topic["proto_file"])
                    {
                        std::string name = topic["name"].as<std::string>();
                        std::string type = topic["type"].as<std::string>();
                        std::string proto_file = topic["proto_file"].as<std::string>();
                        m_recorderConfig.topics.push_back(TopicInfo{name, type, proto_file});
                    }
                }
            }

            return true;
        } catch (const YAML::Exception& e)
        {
            return false;
        }
    }

    /**
     * @brief 从YAML文件加载播放配置
     * @param config_file 配置文件路径
     * @return 是否加载成功
     */
    bool LoadPlayerConfig(const std::string& config_file)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(config_file);

            // 解析输入路径
            if (config["input_path"])
            {
                m_playerConfig.input_path = config["input_path"].as<std::string>();
            }

            // 解析是否循环播放
            if (config["loop_playback"])
            {
                m_playerConfig.loop_playback = config["loop_playback"].as<bool>();
            }

            // 解析播放速率
            if (config["playback_rate"])
            {
                m_playerConfig.playback_rate = config["playback_rate"].as<double>();
            }

            return true;
        } catch (const YAML::Exception& e)
        {
            return false;
        }
    }

    /**
     * @brief 从YAML文件加载存储配置
     * @param config_file 配置文件路径
     * @return 是否加载成功
     */
    bool LoadStorageConfig(const std::string& config_file)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(config_file);

            // 解析proto搜索路径
            if (config["format"] && config["format"]["search_paths"] && config["format"]["search_paths"].IsSequence())
            {
                m_storageConfig.proto_search_paths.clear();
                for (const auto& path : config["format"]["search_paths"])
                {
                    m_storageConfig.proto_search_paths.push_back(path.as<std::string>());
                }
            }

            // 解析压缩配置
            if (config["compression"])
            {
                if (config["compression"]["type"])
                {
                    std::string type = config["compression"]["type"].as<std::string>();
                    if (type == "none")
                    {
                        m_storageConfig.compression_type = CompressionType::NONE;
                    } else if (type == "lz4")
                    {
                        m_storageConfig.compression_type = CompressionType::LZ4;
                    } else if (type == "zstd")
                    {
                        m_storageConfig.compression_type = CompressionType::ZSTD;
                    }
                }

                if (config["compression"]["level"])
                {
                    m_storageConfig.compression_level = config["compression"]["level"].as<int>();
                }
            }

            // 解析写入批次大小
            if (config["write_batch_size"])
            {
                m_storageConfig.write_batch_size = config["write_batch_size"].as<size_t>();
            }

            // 解析最大文件大小
            if (config["max_file_size"])
            {
                m_storageConfig.max_file_size *= config["max_file_size"].as<uint64_t>();
            }

            if (config["chunk_size"])
            {
                m_storageConfig.chunk_size *= config["chunk_size"].as<uint64_t>();
            }

            // 解析是否按大小分割文件
            if (config["split_by_size"])
            {
                m_storageConfig.split_by_size = config["split_by_size"].as<bool>();
            }
            return true;
        } catch (const YAML::Exception& e)
        {
            return false;
        }
    }

    bool LoadBufferConfig(const std::string& config_file)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(config_file);

            if (config["buffer_size"])
            {
                m_bufferConfig.buffer_size = config["buffer_size"].as<size_t>();
            }

            return true;
        } catch (const YAML::Exception& e)
        {
            return false;
        }
    }

    /**
     * @brief 获取录制配置
     * @return 录制配置
     */
    const RecorderConfig& GetRecorderConfig() const { return m_recorderConfig; }

    /**
     * @brief 获取播放配置
     * @return 播放配置
     */
    const PlayerConfig& GetPlayerConfig() const { return m_playerConfig; }

    /**
     * @brief 获取缓冲区配置
     * @return 缓冲区配置
     */
    const BufferConfig& GetBufferConfig() const { return m_bufferConfig; }

    /**
     * @brief 获取存储配置
     * @return 存储配置
     */
    const StorageConfig& GetStorageConfig() const { return m_storageConfig; }

    /**
     * @brief 设置录制配置
     * @param config 录制配置
     */
    void SetRecorderConfig(const RecorderConfig& config) { m_recorderConfig = config; }

    /**
     * @brief 设置播放配置
     * @param config 播放配置
     */
    void SetPlayerConfig(const PlayerConfig& config) { m_playerConfig = config; }

    /**
     * @brief 设置存储配置
     * @param config 存储配置
     */
    void SetStorageConfig(const StorageConfig& config) { m_storageConfig = config; }

private:
    RecorderConfig m_recorderConfig;  ///< 录制配置
    PlayerConfig m_playerConfig;      ///< 播放配置
    StorageConfig m_storageConfig;    ///< 存储配置
    BufferConfig m_bufferConfig;      ///< 缓冲区配置
};

using ConfigManagerPtr = std::shared_ptr<ConfigManager>;

}  // namespace openbag
