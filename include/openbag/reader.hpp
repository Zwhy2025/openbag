/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file mcap_reader.hpp
 * @brief MCAP 格式文件读取器，支持 Protobuf 消息的动态解析 - 纯hpp实现
 */

#pragma once

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "mcap/reader.hpp"

namespace openbag {

/**
 * @brief MCAP 读取器类，支持 Protobuf 消息动态解析 - 简化版实现
 */
class Reader
{
public:
    /**
     * @brief 构造函数
     */
    Reader() : m_isOpen(false)
    {
        // 延迟初始化protobuf相关对象
    }

    /**
     * @brief 析构函数
     */
    ~Reader() { Close(); }

    /**
     * @brief 打开 MCAP 文件
     * @param filename 文件名
     * @return 是否成功
     */
    bool Open(const std::string &filename)
    {
        if (m_isOpen)
        {
            return false;
        }

        const auto res = m_reader.open(filename);
        if (!res.ok())
        {
            std::cerr << "Failed to open " << filename << " for reading: " << res.message << std::endl;
            return false;
        }

        // 读取摘要信息，这样才能获取到channels等元数据
        const auto summaryRes = m_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
        if (!summaryRes.ok())
        {
            std::cerr << "Failed to read summary from " << filename << ": " << summaryRes.message << std::endl;
            return false;
        }

        m_isOpen = true;
        return true;
    }

    /**
     * @brief 关闭文件
     */
    void Close()
    {
        if (m_isOpen)
        {
            m_reader.close();
            m_isOpen = false;
        }
    }

    /**
     * @brief 获取所有话题列表
     * @return 话题列表
     */
    std::vector<std::string> GetTopics()
    {
        std::vector<std::string> topics;
        if (!m_isOpen)
        {
            return topics;
        }

        // 从channels获取话题
        for (const auto &[channelId, channel] : m_reader.channels())
        {
            topics.push_back(channel->topic);
        }

        return topics;
    }
    /**
     * @brief 获取流式消息视图，用于按需读取消息
     * @return 消息视图
     */
    auto GetMessages()
    {
        if (!m_isOpen)
        {
            // 返回默认构造的空消息视图
            return mcap::McapReader{}.readMessages();
        }
        return m_reader.readMessages();
    }

    /**
     * @brief 获取通道信息
     * @return 通道映射
     */
    auto GetChannels() const
    {
        if (!m_isOpen)
        {
            // 返回空的通道映射
            return std::unordered_map<mcap::ChannelId, mcap::ChannelPtr>{};
        }
        return m_reader.channels();
    }

private:
    bool m_isOpen;              ///< 是否已打开
    mcap::McapReader m_reader;  ///< MCAP 读取器
};

using ReaderPtr = std::unique_ptr<Reader>;

}  // namespace openbag