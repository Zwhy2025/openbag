/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file storage.hpp
 * @brief Protobuf MCAP存储实现
 */

#pragma once

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>

#include <filesystem>
#include <mcap/mcap.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common.hpp"
#include "config.hpp"
#include "openbag/proto_utils.hpp"

namespace openbag {

/**
 * @brief Protobuf MCAP存储实现类
 */
class Storage
{
public:
    /**
     * @brief 构造函数
     * @param config 配置指针
     */
    explicit Storage(const StorageConfig& config) : m_config(config) { m_importer = CreateProtoImporter(m_config.proto_search_paths); }

    /**
     * @brief 析构函数
     */
    ~Storage() { Close(); }

    bool GenFilename(FileInfo& fileInfo)
    {
        // 如果路径为空，使用固定文件
        if (fileInfo.output_path.empty())
        {
            fileInfo.filename = "./openbags/" + fileInfo.prefix + "." + fileInfo.extension;
        } else
        {
            // 如果路径末尾没有斜杠，则加上
            if (fileInfo.output_path.back() != '/')
            {
                fileInfo.output_path += '/';
            }

            fileInfo.filename = fileInfo.output_path + GenerateUniqueFilename(fileInfo.prefix, fileInfo.extension);
        }
        return true;
    }
    mcap::McapWriterOptions CreateWriterOptions()
    {
        // 设置MCAP写入选项
        mcap::McapWriterOptions writerOptions("");

        // 根据配置设置压缩选项
        switch (m_config.comperssion_type)
        {
            case CompressionType::NONE:
                writerOptions.compression = mcap::Compression::None;
                break;
            case CompressionType::LZ4:
                writerOptions.compression = mcap::Compression::Lz4;
                break;
            case CompressionType::ZSTD:
                writerOptions.compression = mcap::Compression::Zstd;
                break;
        }

        writerOptions.compressionLevel = static_cast<mcap::CompressionLevel>(m_config.comperssion_level);

        writerOptions.chunkSize = m_config.chunk_size;  // 1MB
        return writerOptions;
    }
    /**
     * @brief 打开存储
     * @param filename 文件名
     * @return 是否成功
     */
    bool Open(FileInfo& fileInfo)
    {
        if (m_fileInfo.is_open)
        {
            return false;
        }

        if (!GenFilename(fileInfo))
        {
            return false;
        }
        std::cout << "-------------------------------------------------------" << std::endl;
        std::cout << "start bag file path: " << fileInfo.filename << std::endl;
        std::cout << "-------------------------------------------------------" << std::endl;

        std::filesystem::path filePath(fileInfo.filename);

        std::filesystem::create_directories(filePath.parent_path());
        mcap::McapWriterOptions writerOptions = CreateWriterOptions();

        // 创建MCAP写入器
        m_writer = std::make_unique<mcap::McapWriter>();

        // 打开文件
        const auto status = m_writer->open(fileInfo.filename, writerOptions);
        if (!status.ok())
        {
            return false;
        }

        fileInfo.is_open = true;
        fileInfo.file_size = 0;
        m_fileInfo = fileInfo;
        m_topicInfos.clear();
        return true;
    }

    /**
     * @brief 关闭存储
     */
    void Close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_fileInfo.is_open)
        {
            return;
        }

        try
        {
            if (m_writer)
            {
                // 首先调用close方法
                m_writer->close();

                // 然后重置指针
                m_writer.reset();
            }
        } catch (const std::exception& e)
        {
            std::cerr << "Unknown exception occurred while closing MCAP file" << std::endl;
            // 即使发生异常，也继续处理
        } catch (...)
        {
            std::cerr << "Unknown exception occurred while closing MCAP file" << std::endl;
        }

        m_fileInfo.is_open = false;
        m_fileInfo.file_size = 0;
    }

    /**
     * @brief 导入Proto文件
     * @param proto_file proto文件路径（相对于搜索路径的路径）
     * @return 是否成功
     */
    bool ImportProtoFile(const std::string& proto_file)
    {
        const google::protobuf::FileDescriptor* file = (*m_importer)->Import(proto_file);
        return file != nullptr;
    }

    /**
     * @brief 注册主题和消息类型
     * @param topic 主题名称
     * @param message_type 消息类型
     * @return 是否成功
     */
    bool RegisterTopic(TopicInfo& topicInfo)
    {
        std::string protoFile = topicInfo.proto_file;
        if (!this->ImportProtoFile(protoFile))
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        return RegisterTopicImpl(topicInfo);
    }

    /**
     * @brief 写入消息
     * @param message 消息指针
     * @return 是否成功
     */
    bool WriteMessage(const MessagePtr& message)
    {
        if (!m_fileInfo.is_open || !message)
        {
            std::cerr << "写入消息失败: 存储未打开或消息为空" << std::endl;
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        bool result = WriteSingleMessage(message);
        if (!result) return false;

        TrySplitFileIfNeeded();
        return true;
    }

    /**
     * @brief 写入消息批次
     * @param messages 消息列表
     * @return 是否成功
     */
    bool WriteMessageBatch(const std::vector<MessagePtr>& messages)
    {
        if (!m_fileInfo.is_open || messages.empty())
        {
            std::cerr << "写入消息批次失败: 存储未打开或消息列表为空" << std::endl;
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        bool allSuccess = true;
        for (const auto& message : messages)
        {
            if (!message) continue;

            if (!WriteSingleMessage(message))
            {
                allSuccess = false;
            }
        }

        TrySplitFileIfNeeded();
        return allSuccess;
    }

    /**
     * @brief 读取消息
     * @param topic 话题名称，为空表示所有话题
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @return 消息列表
     */
    std::vector<MessagePtr> ReadMessages(const std::string& topic, int64_t startTime, int64_t endTime)
    {
        // 需要实际实现读取MCAP文件的逻辑
        return {};
    }

    /**
     * @brief 获取存储文件当前大小
     * @return 文件大小(字节)
     */
    uint64_t GetFileSize() const { return m_fileInfo.file_size; }

    /**
     * @brief 获取话题列表
     * @return 话题列表
     */
    std::vector<std::string> GetTopics() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> topics;
        for (const auto& topicInfo : m_topicInfos)
        {
            topics.push_back(topicInfo.second.topic_name);
        }
        return topics;
    }

private:
    bool RegisterTopicImpl(TopicInfo& topicInfo)
    {
        // 查找或创建消息类型描述符
        const google::protobuf::Descriptor* descriptor = (*m_importer)->pool()->FindMessageTypeByName(topicInfo.proto_type);
        if (!descriptor)
        {
            std::cerr << "无法找到消息类型: " << topicInfo.proto_type << std::endl;
            return false;
        }

        // 构建文件描述符集
        auto fileDescriptorSet = BuildFileDescriptorSet(descriptor);

        // 添加Schema
        mcap::Schema schema;
        schema.name = topicInfo.proto_type;
        schema.encoding = "protobuf";
        schema.id = m_topicInfos.size() + 1;

        // 序列化文件描述符集
        std::string data;
        if (!fileDescriptorSet.SerializeToString(&data))
        {
            std::cerr << "序列化文件描述符集失败" << std::endl;
            return false;
        }

        // 设置schema数据
        schema.data.assign(reinterpret_cast<const std::byte*>(data.data()), reinterpret_cast<const std::byte*>(data.data() + data.size()));

        // 添加Schema
        m_writer->addSchema(schema);

        // 添加Channel
        mcap::Channel channel;
        channel.topic = topicInfo.topic_name;
        channel.messageEncoding = "protobuf";
        channel.schemaId = schema.id;
        channel.id = topicInfo.channel_id;
        channel.metadata["message_type"] = topicInfo.proto_type;

        // 添加Channel
        m_writer->addChannel(channel);

        topicInfo.schema_id = schema.id;
        topicInfo.channel_id = channel.id;
        m_topicInfos[topicInfo.topic_name] = topicInfo;
        std::cout << "Proto类型注册成功: " << topicInfo.topic_name << " -> " << topicInfo.proto_type << std::endl;
        return true;
    }

    bool WriteSingleMessage(const MessagePtr& message)
    {
        if (!message)
        {
            std::cerr << "消息为空，无法写入" << std::endl;
            return false;
        }

        auto it = m_topicInfos.find(message->topic);
        if (it == m_topicInfos.end())
        {
            std::cerr << "写入消息失败: 找不到主题对应的Channel ID: " << message->topic << std::endl;
            return false;
        }
        mcap::ChannelId channelId = it->second.channel_id;

        // 创建MCAP消息
        mcap::Message mcapMsg;
        mcapMsg.channelId = channelId;
        mcapMsg.sequence = message->sequence_number;
        mcapMsg.logTime = message->timestamp * 1000;      // 微秒转纳秒
        mcapMsg.publishTime = message->timestamp * 1000;  // 微秒转纳秒
        mcapMsg.data = reinterpret_cast<const std::byte*>(message->data.data());
        mcapMsg.dataSize = message->data.size();

        // 写入消息
        const auto status = m_writer->write(mcapMsg);
        if (!status.ok())
        {
            std::cerr << "写入MCAP消息失败: " << status.message << std::endl;
            return false;
        }

        // 更新文件大小
        m_fileInfo.file_size +=
            mcapMsg.dataSize + sizeof(mcapMsg.channelId) + sizeof(mcapMsg.sequence) + sizeof(mcapMsg.logTime) + sizeof(mcapMsg.publishTime) + sizeof(mcapMsg.dataSize);

        return true;
    }

    void TrySplitFileIfNeeded()
    {
        if (m_config.split_by_size && m_fileInfo.file_size >= m_config.max_file_size)
        {
            std::cout << "文件大小超过限制，创建新文件..." << std::endl;
            // 关闭当前文件
            m_writer->close();

            FileInfo newFileInfo(m_fileInfo);
            if (!GenFilename(newFileInfo))
            {
                return;
            }

            auto writerOptions = CreateWriterOptions();
            const auto openStatus = m_writer->open(newFileInfo.filename, writerOptions);
            if (!openStatus.ok())
            {
                throw std::runtime_error("Failed to open new MCAP file: " + openStatus.message);
            }

            m_fileInfo = newFileInfo;
            m_fileInfo.file_size = 0;
            m_fileInfo.is_open = true;

            // 重新注册所有主题和消息类型
            for (auto& [topic, info] : m_topicInfos)
            {
                RegisterTopicImpl(info);
            }
        }
    }

private:
    FileInfo m_fileInfo;
    StorageConfig m_config;                      ///< 配置
    std::unique_ptr<mcap::McapWriter> m_writer;  ///< MCAP写入器

    std::unordered_map<std::string, TopicInfo> m_topicInfos;  ///< 话题信息映射
    std::unique_ptr<ProtoImporterWrapper> m_importer;
    mutable std::mutex m_mutex;  ///< 互斥锁
};

using StoragePtr = std::shared_ptr<Storage>;

}  // namespace openbag