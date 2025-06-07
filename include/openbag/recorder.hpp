/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file recorder.hpp
 * @brief 录制器
 */

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "openbag/buffer.hpp"
#include "openbag/common.hpp"
#include "openbag/config.hpp"
#include "openbag/storage.hpp"
#include "openbag/transport.hpp"
namespace openbag {

/**
 * @brief 录制状态枚举
 */
enum class RecorderState
{
    STOPPED,  ///< 已停止
    RUNNING,  ///< 运行中
    PAUSED    ///< 已暂停
};

/**
 * @brief 录制器类
 */
class Recorder
{
    using PatternStr = std::string;
    using SubscriberFunc = std::function<std::shared_ptr<OpenbagSubscriberBase>(const std::string &)>;

public:
    /**
     * @brief 构造函数
     * @param config 配置
     * @param adapterFactory 消息适配器工厂
     */
    explicit Recorder(const ConfigManager &configManager, MessageAdapterFactoryPtr adapterFactory = nullptr, SubscriberFunc subscriberFunc = nullptr)
        : m_configManager(configManager),
          m_config(configManager.GetRecorderConfig()),
          m_adapterFactory(adapterFactory),
          m_subscriberFunc(subscriberFunc),
          m_storage(std::make_shared<Storage>(m_configManager.GetStorageConfig())),
          m_buffer(std::make_shared<MessageBuffer>(m_configManager.GetBufferConfig()))
    {
        // 如果订阅者函数为空，则使用默认订阅者函数
        if (!m_subscriberFunc)
        {
            m_subscriberFunc = [this](const std::string &topic) { return this->DefaultSubscriptionCallback(topic); };
        }

        if (!m_adapterFactory)
        {
            throw std::runtime_error("MessageAdapterFactory is nullptr");
        }
    }

    void SetSubscriberFunc(SubscriberFunc subscriberFunc) { m_subscriberFunc = subscriberFunc; }
    /**
     * @brief 析构函数
     */
    ~Recorder() { Stop(); }

    /**
     * @brief 启动录制
     * @return 是否成功启动
     */
    bool Start()
    {
        if (m_state == RecorderState::RUNNING)
        {
            return true;  // 已经在运行
        }

        if (m_config.topics.empty())
        {
            return false;  // 没有配置话题
        }

        FileInfo fileInfo;

        fileInfo.prefix = m_config.filename_prefix;
        fileInfo.extension = m_config.output_format;
        fileInfo.output_path = m_config.output_path;

        // 打开存储
        if (!m_storage->Open(fileInfo))
        {
            return false;
        }
        // 启动缓冲区
        m_buffer->Clear();
        m_totalMessages = 0;
        // 设置状态为运行中
        m_state = RecorderState::RUNNING;
        m_lastSnapshotTime = GetCurrentTimestampUs();

        // 创建订阅者
        m_subscribers.clear();

        m_buffer->Start();

        // 处理所有话题
        for (auto &topic : m_config.topics)
        {
            if (!m_storage->RegisterTopic(topic))
            {
                std::cerr << "注册话题和消息类型失败: " << topic.topic_name << " -> " << topic.proto_type << std::endl;

                m_running = false;
                m_state = RecorderState::STOPPED;
                if (m_buffer) m_buffer->Stop();
                if (m_storage) m_storage->Close();

                return false;
            }

            // 使用默认字符串订阅者
            auto subscriber = m_subscriberFunc(topic.topic_name);
            if (subscriber)
            {
                m_subscribers[topic.topic_name] = subscriber;
            }
        }

        // 启动写入线程
        m_running = true;
        m_writeThread = std::thread(&Recorder::WriteLoop, this);

        return true;
    }

    /**
     * @brief 默认订阅者
     * @param topic 话题名称
     * @return 订阅者
     */
    std::shared_ptr<OpenbagSubscriberBase> DefaultSubscriptionCallback(const std::string &topic)
    {
        auto subscriber = m_adapterFactory->CreateSubscriber<std::string>(topic, [this, topic](const std::string &data) {
            // 发送到缓冲区
            this->OnMessageReceived(topic, data);
        });
        return subscriber;
    }

    /**
     * @brief 停止录制
     */
    void Stop()
    {
        try
        {
            if (m_state == RecorderState::STOPPED)
            {
                return;  // 已经停止
            }

            std::cout << "正在停止录制器..." << std::endl;

            // 设置状态为已停止
            m_state = RecorderState::STOPPED;

            // 1. 先停止订阅者，不再接收新消息
            std::cout << "清理订阅者..." << std::endl;
            try
            {
                m_subscribers.clear();
            } catch (const std::exception &e)
            {
                std::cerr << "清理订阅者时发生异常: " << e.what() << std::endl;
            }

            // 2. 记录当前缓冲区中剩余的消息数量
            size_t remainingMessages = 0;
            try
            {
                remainingMessages = m_buffer->Size();
                if (remainingMessages > 0)
                {
                    std::cout << "缓冲区中有 " << remainingMessages << " 条消息等待写入..." << std::endl;
                }
            } catch (const std::exception &e)
            {
                std::cerr << "获取缓冲区大小时发生异常: " << e.what() << std::endl;
            }

            // 3. 通知写入线程退出，但会先处理完缓冲区数据
            std::cout << "等待写入线程处理缓冲区数据..." << std::endl;
            m_running = false;

            // 4. 等待写入线程完成
            if (m_writeThread.joinable())
            {
                try
                {
                    std::cout << "等待写入线程结束..." << std::endl;
                    m_writeThread.join();  // 直接等待线程完成，不设置超时
                    std::cout << "写入线程已完成并退出" << std::endl;
                } catch (const std::exception &e)
                {
                    std::cerr << "等待写入线程时发生异常: " << e.what() << std::endl;
                }
            }

            // 5. 停止缓冲区
            std::cout << "停止缓冲区..." << std::endl;
            try
            {
                if (m_buffer)
                {
                    m_buffer->Stop();
                }
            } catch (const std::exception &e)
            {
                std::cerr << "停止缓冲区时发生异常: " << e.what() << std::endl;
            }

            // 6. 关闭存储
            std::cout << "关闭存储..." << std::endl;
            try
            {
                if (m_storage)
                {
                    m_storage->Close();
                }
            } catch (const std::exception &e)
            {
                std::cerr << "关闭存储时发生异常: " << e.what() << std::endl;
            }

            std::cout << "录制器已完全停止" << std::endl;
        } catch (const std::exception &e)
        {
            std::cerr << "停止录制器时发生异常: " << e.what() << std::endl;
        } catch (...)
        {
            std::cerr << "停止录制器时发生未知异常" << std::endl;
        }
    }

    /**
     * @brief 暂停录制
     */
    void Pause()
    {
        if (m_state != RecorderState::RUNNING)
        {
            return;
        }

        m_state = RecorderState::PAUSED;
    }

    /**
     * @brief 恢复录制
     */
    void Resume()
    {
        if (m_state != RecorderState::PAUSED)
        {
            return;
        }

        m_state = RecorderState::RUNNING;
    }

    int GetWriteBatchSize() const { return m_configManager.GetStorageConfig().write_batch_size; }

    /**
     * @brief 获取录制状态
     * @return 录制状态
     */
    RecorderState GetState() const { return m_state; }

    /**
     * @brief 获取已记录的消息数量
     * @return 消息数量
     */
    uint64_t GetTotalMessages() const { return m_totalMessages; }

    /**
     * @brief 获取当前文件大小
     * @return 文件大小(字节)
     */
    uint64_t GetFileSize() const { return m_storage->GetFileSize(); }

    /**
     * @brief 获取录制的话题列表
     * @return 话题列表
     */
    std::vector<std::string> GetTopics() const
    {
        std::vector<std::string> topics;
        for (const auto &[topic, _] : m_subscribers)
        {
            topics.push_back(topic);
        }
        return topics;
    }

    /**
     * @brief 消息接收回调
     * @param topic 话题名称
     * @param message 消息内容
     */
    void OnMessageReceived(const std::string &topic, const std::string &message)
    {
        if (m_state != RecorderState::RUNNING)
        {
            return;  // 非运行状态不记录消息
        }

        // 获取当前时间戳
        int64_t timestamp = GetCurrentTimestampUs();

        // 添加到缓冲区
        if (m_buffer->PushMessage(topic, message, timestamp))
        {
            // 记录总消息数
            m_totalMessages++;
        }
    }

private:
    /**
     * @brief 话题配置信息
     */
    struct TopicConfig
    {
        std::string type_name;                             ///< 类型名称
        std::function<void(const std::string &)> handler;  ///< 消息处理函数
    };

    // 序列化器类型，接收void*指针（可以是任何类型）返回序列化字符串
    using SerializerFunc = std::function<std::string(const void *)>;

    /**
     * @brief 写入线程
     */
    void WriteLoop()
    {
        try
        {
            std::cout << "写入线程已启动" << std::endl;
            std::vector<MessagePtr> batch;

            while (m_running || m_buffer->Size() > 0)
            {
                // 从缓冲区取出一批消息
                batch.clear();

                // 如果停止状态，尝试一次性获取所有剩余消息
                size_t batchSize = m_running ? this->GetWriteBatchSize() : m_buffer->Size();

                if (m_buffer->PopMessages(batch, batchSize))
                {
                    // 写入消息批次
                    try
                    {
                        if (!m_storage->WriteMessageBatch(batch))
                        {
                            std::cerr << "写入消息批次失败" << std::endl;
                        } else if (!m_running)
                        {
                            // 如果在停止过程中，打印进度
                            std::cout << "成功写入 " << batch.size() << " 条消息，缓冲区剩余 " << m_buffer->Size() << " 条" << std::endl;
                        }
                    } catch (const std::exception &e)
                    {
                        std::cerr << "写入消息时发生异常: " << e.what() << std::endl;
                    }
                } else if (m_running)
                {
                    // 运行状态下没有消息时短暂休眠
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            std::cout << "写入线程已完成所有数据处理并退出" << std::endl;
        } catch (const std::exception &e)
        {
            std::cerr << "写入线程发生异常: " << e.what() << std::endl;
        } catch (...)
        {
            std::cerr << "写入线程发生未知异常" << std::endl;
        }
    }

private:
    /**  */
    ConfigManager m_configManager;  ///< 配置管理器
    RecorderConfig m_config;        ///< 录制配置
    /**  */
    MessageBufferPtr m_buffer;  ///< 消息缓冲区
    StoragePtr m_storage;       ///< 存储接口
    /**  */
    MessageAdapterFactoryPtr m_adapterFactory;                            ///< 消息适配器工厂
    SubscriberFunc m_subscriberFunc;                                      ///< 订阅者函数
    std::unordered_map<std::string, OpenbagSubscriberPtr> m_subscribers;  ///< 订阅者映射
    /**  */
    std::atomic<RecorderState> m_state{RecorderState::STOPPED};  ///< 录制状态
    std::atomic<uint64_t> m_totalMessages{0};                    ///< 总消息数
    std::atomic<int64_t> m_lastSnapshotTime{0};                  ///< 最后快照时间
    std::atomic<bool> m_running{false};                          ///< 线程运行标志
    /**  */
    std::thread m_writeThread;  ///< 写入线程
};

}  // namespace openbag
