/**
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file buffer.hpp
 * @brief 线程安全的消息缓冲队列实现
 */

#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "mcap/writer.hpp"
#include "openbag/common.hpp"
#include "openbag/config.hpp"

namespace openbag {

/**
 * @brief 线程安全的环形消息缓冲队列
 */
class MessageBuffer
{
public:
    /**
     * @brief 构造函数
     * @param max_queue_size 最大队列大小
     */
    explicit MessageBuffer(const BufferConfig& config) : m_config(config), m_maxQueueSize(config.buffer_size), m_running(true), m_totalMessages(0) {}

    /**
     * @brief 析构函数
     */
    ~MessageBuffer() { Stop(); }

    /**
     * @brief 添加消息到缓冲区
     * @param topic 话题名称
     * @param data 消息数据
     * @param timestamp 时间戳(微秒)
     * @return 是否添加成功
     */
    bool PushMessage(const std::string& topic, const std::string& data, int64_t timestamp)
    {
        if (!m_running)
        {
            return false;
        }

        std::unique_lock<std::mutex> lock(m_mutex);

        // 检查队列是否已满
        if (m_messageQueue.size() >= m_maxQueueSize)
        {
            // 队列已满，等待队列空间释放
            if (!m_notFull.wait_for(lock, std::chrono::milliseconds(100), [this] { return m_messageQueue.size() < m_maxQueueSize || !m_running; }))
            {
                std::cerr << "out max buff size" << std::endl;
                return false;  // 超时或非运行状态
            }

            if (!m_running)
            {
                return false;
            }
        }

        // 创建消息并添加到队列
        MessagePtr message = std::make_shared<Message>();
        message->topic = topic;
        message->data = data;
        message->timestamp = timestamp;
        message->sequence_number = m_totalMessages++;

        m_messageQueue.push_back(message);
        m_topicQueues[topic].push_back(message);

        // 通知等待的消费者
        lock.unlock();
        m_notEmpty.notify_one();

        return true;
    }

    /**
     * @brief 从缓冲区获取一组消息并填充到提供的向量中
     * @param[out] messages 用于接收消息的向量
     * @param max_batch_size 最大批量大小
     * @param timeout_ms 超时时间(毫秒)
     * @return 是否获取到消息
     */
    bool PopMessages(std::vector<MessagePtr>& messages, size_t max_batch_size, int timeout_ms = 100)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_messageQueue.empty() && m_running)
        {
            // 等待消息到达或超时
            m_notEmpty.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !m_messageQueue.empty() || !m_running; });
        }

        // 只要队列不为空，即使系统已停止，也应该返回消息
        if (m_messageQueue.empty())
        {
            return false;
        }

        // 取出批量消息
        size_t count = std::min(max_batch_size, m_messageQueue.size());
        for (size_t i = 0; i < count; ++i)
        {
            MessagePtr message = m_messageQueue.front();
            m_messageQueue.pop_front();

            // 同时从话题队列中移除
            auto& topicQueue = m_topicQueues[message->topic];
            if (!topicQueue.empty())
            {
                topicQueue.pop_front();
                if (topicQueue.empty())
                {
                    m_topicQueues.erase(message->topic);
                }
            }

            messages.push_back(message);
        }

        // 通知等待的生产者
        lock.unlock();
        m_notFull.notify_one();

        return !messages.empty();
    }

    /**
     * @brief 从特定话题的缓冲区获取一组消息
     * @param topic 话题名称
     * @param max_batch_size 最大批量大小
     * @param timeout_ms 超时时间(毫秒)
     * @return 消息列表
     */
    std::vector<MessagePtr> PopMessagesByTopic(const std::string& topic, size_t max_batch_size, int timeout_ms = 100)
    {
        std::vector<MessagePtr> batch;
        std::unique_lock<std::mutex> lock(m_mutex);

        auto it = m_topicQueues.find(topic);
        if (it == m_topicQueues.end() && m_running)
        {
            // 等待特定话题的消息到达或超时
            m_notEmpty.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, &topic] { return m_topicQueues.find(topic) != m_topicQueues.end() || !m_running; });

            it = m_topicQueues.find(topic);
        }

        // 如果没有该话题的消息或系统已停止，返回空批次
        if (it == m_topicQueues.end() || !m_running)
        {
            return batch;
        }

        auto& topicQueue = it->second;

        // 取出批量消息
        size_t count = std::min(max_batch_size, topicQueue.size());
        for (size_t i = 0; i < count; ++i)
        {
            MessagePtr message = topicQueue.front();
            topicQueue.pop_front();

            // 从主队列中查找并移除该消息
            for (auto iter = m_messageQueue.begin(); iter != m_messageQueue.end(); ++iter)
            {
                if (*iter == message)
                {
                    m_messageQueue.erase(iter);
                    break;
                }
            }

            batch.push_back(message);
        }

        // 如果话题队列为空，移除该话题
        if (topicQueue.empty())
        {
            m_topicQueues.erase(topic);
        }

        // 通知等待的生产者
        lock.unlock();
        m_notFull.notify_one();

        return batch;
    }

    /**
     * @brief 获取缓冲区中的消息数量
     * @return 消息数量
     */
    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messageQueue.size();
    }

    /**
     * @brief 获取特定话题缓冲区中的消息数量
     * @param topic 话题名称
     * @return 消息数量
     */
    size_t TopicSize(const std::string& topic) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_topicQueues.find(topic);
        if (it == m_topicQueues.end())
        {
            return 0;
        }
        return it->second.size();
    }

    /**
     * @brief
     *
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messageQueue.clear();
        m_topicQueues.clear();
    }

    /**
     * @brief 停止缓冲区操作
     */
    void Stop()
    {
        m_running = false;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

    void Start()
    {
        m_running = true;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

    /**
     * @brief 检查缓冲区是否在运行
     * @return 是否在运行
     */
    bool IsRunning() const { return m_running; }

private:
    BufferConfig m_config;  ///< 配置

    std::deque<MessagePtr> m_messageQueue;                                  ///< 主消息队列
    std::unordered_map<std::string, std::deque<MessagePtr>> m_topicQueues;  ///< 按话题分类的消息队列

    size_t m_maxQueueSize;                  ///< 最大队列大小
    std::atomic<bool> m_running;            ///< 运行状态标志
    std::atomic<uint64_t> m_totalMessages;  ///< 总消息计数

    mutable std::mutex m_mutex;          ///< 互斥锁
    std::condition_variable m_notEmpty;  ///< 非空条件变量
    std::condition_variable m_notFull;   ///< 非满条件变量
};

using MessageBufferPtr = std::shared_ptr<MessageBuffer>;

}  // namespace openbag
