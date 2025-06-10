/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file player.hpp
 * @brief 回放器
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "openbag/config.hpp"
#include "openbag/reader.hpp"
#include "openbag/transport.hpp"

namespace openbag {

/**
 * @brief 播放状态枚举
 */
enum class PlayerState
{
    STOPPED,  ///< 已停止
    PLAYING,  ///< 播放中
    PAUSED    ///< 已暂停
};

/**
 * @brief 播放器类，用于回放记录文件
 */
class Player
{
    using PublisherFunc = std::function<std::shared_ptr<OpenbagPublisherBase>(const std::string&)>;

public:
    /**
     * @brief 构造函数
     * @param config 播放配置
     * @param adapterFactory 消息适配器工厂
     * @param publisherFunc 发布者创建函数
     */
    explicit Player(const PlayerConfig& config, MessageAdapterFactoryPtr adapterFactory = nullptr, PublisherFunc publisherFunc = nullptr)
        : m_config(config), m_state(PlayerState::STOPPED), m_running(false), m_playedMessages(0), m_adapterFactory(adapterFactory), m_publisherFunc(publisherFunc)
    {
        if (!m_publisherFunc)
        {
            m_publisherFunc = [this](const std::string& topic) { return this->DefaultPublisherCallback(topic); };
        }

        if (!m_adapterFactory)
        {
            throw std::runtime_error("MessageAdapterFactory is nullptr");
        }
    }

    /**
     * @brief 析构函数
     */
    ~Player() { Stop(); }

    /**
     * @brief 设置发布者函数
     * @param publisherFunc 发布者创建函数
     */
    void SetPublisherFunc(PublisherFunc publisherFunc) { m_publisherFunc = publisherFunc; }

    /**
     * @brief 启动播放
     * @return 是否成功启动
     */
    bool Start()
    {
        if (m_state == PlayerState::PLAYING)
        {
            return true;  // 已经在播放
        }

        if (m_config.input_path.empty())
        {
            return false;  // 未指定输入文件
        }

        // 创建MCAP读取器
        m_mcapReader = std::make_unique<Reader>();
        if (!m_mcapReader)
        {
            return false;
        }

        // 打开MCAP文件
        if (!m_mcapReader->Open(m_config.input_path))
        {
            return false;
        }

        // 读取所有可用话题
        auto availableTopics = m_mcapReader->GetTopics();
        if (availableTopics.empty())
        {
            return false;  // 没有可用话题
        }

        // 创建话题发布者
        m_publishers.clear();
        for (const auto& topic : availableTopics)
        {
            std::string publishTopic = topic;

            // 使用发布者函数创建发布者
            auto publisher = m_publisherFunc(publishTopic);
            if (publisher)
            {
                m_publishers[topic] = publisher;
            }
        }

        // 重置计数
        m_playedMessages = 0;

        // 设置状态为播放中
        m_state = PlayerState::PLAYING;

        // 启动播放线程
        m_running = true;
        m_playThread = std::thread(&Player::PlayLoop, this);

        return true;
    }

    /**
     * @brief 默认发布者回调
     * @param topic 话题名称
     * @return 发布者
     */
    std::shared_ptr<OpenbagPublisherBase> DefaultPublisherCallback(const std::string& topic) { return m_adapterFactory->CreatePublisher(topic); }

    /**
     * @brief 停止播放
     */
    void Stop()
    {
        if (m_state == PlayerState::STOPPED)
        {
            return;  // 已经停止
        }

        // 设置状态为已停止
        m_state = PlayerState::STOPPED;

        // 停止播放线程
        m_running = false;
        m_playPauseCV.notify_all();
        if (m_playThread.joinable())
        {
            m_playThread.join();
        }

        // 清理发布者
        m_publishers.clear();

        // 关闭MCAP读取器
        if (m_mcapReader)
        {
            m_mcapReader->Close();
        }
    }

    /**
     * @brief 暂停播放
     */
    void Pause()
    {
        if (m_state != PlayerState::PLAYING)
        {
            return;
        }

        m_state = PlayerState::PAUSED;
    }

    /**
     * @brief 恢复播放
     */
    void Resume()
    {
        if (m_state != PlayerState::PAUSED)
        {
            return;
        }

        m_state = PlayerState::PLAYING;
        m_playPauseCV.notify_all();
    }

    /**
     * @brief 获取播放状态
     * @return 播放状态
     */
    PlayerState GetState() const { return m_state; }

    /**
     * @brief 获取已播放的消息数量
     * @return 消息数量
     */
    uint64_t GetPlayedMessages() const { return m_playedMessages; }

    /**
     * @brief 获取可用话题列表
     * @return 话题列表
     */
    std::vector<std::string> GetAvailableTopics() const
    {
        std::vector<std::string> topics;
        for (const auto& [topic, _] : m_publishers)
        {
            topics.push_back(topic);
        }
        return topics;
    }

    /**
     * @brief 设置播放速率
     * @param rate 播放速率，1.0表示正常速度
     */
    void SetPlaybackRate(double rate)
    {
        if (rate <= 0.0)
        {
            rate = 1.0;
        }
        m_config.playback_rate = rate;
    }

    /**
     * @brief 获取播放速率
     * @return 播放速率
     */
    double GetPlaybackRate() const { return m_config.playback_rate; }

private:
    /**
     * @brief 播放线程循环
     */
    void PlayLoop()
    {
        // 检查MCAP读取器是否有效
        if (!m_mcapReader)
        {
            std::cerr << "MCAP读取器无效，停止播放" << std::endl;
            m_state = PlayerState::STOPPED;
            return;
        }

        // 使用流式读取，避免一次性加载所有消息到内存
        auto messageView = m_mcapReader->GetMessages();

        // 获取回放开始的系统时间
        auto playStartTime = std::chrono::steady_clock::now();
        int64_t lastTimestamp = 0;
        bool firstMessage = true;

        // 流式处理消息
        for (auto it = messageView.begin(); it != messageView.end() && m_running; ++it)
        {
            // 检查是否暂停
            if (m_state == PlayerState::PAUSED)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // 记录暂停开始时间
                auto pauseStartTime = std::chrono::steady_clock::now();
                // 等待恢复播放
                m_playPauseCV.wait(lock, [this] { return m_state != PlayerState::PAUSED || !m_running; });

                if (!m_running)
                {
                    break;
                }

                // 计算暂停持续时间并调整播放起始时间
                auto pauseDuration = std::chrono::steady_clock::now() - pauseStartTime;
                playStartTime += pauseDuration;
            }

            // 跳过非 protobuf 编码的消息
            if (!it->schema || it->schema->encoding != "protobuf")
            {
                continue;
            }

            // 获取消息信息
            const auto& mcapMessage = it->message;
            int64_t currentTimestamp = mcapMessage.logTime;

            // 计算时间差并延迟
            if (!firstMessage && m_config.playback_rate > 0)
            {
                int64_t deltaTime = currentTimestamp - lastTimestamp;
                if (deltaTime > 0)
                {
                    auto delayMs = static_cast<int64_t>(deltaTime / 1000000.0 / m_config.playback_rate);
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                }
            }
            firstMessage = false;
            lastTimestamp = currentTimestamp;

            // 查找通道信息获取话题名称
            std::string topic;
            auto channels = m_mcapReader->GetChannels();
            auto channelIt = channels.find(mcapMessage.channelId);
            if (channelIt != channels.end())
            {
                topic = channelIt->second->topic;
            }

            // 如果没有找到话题，跳过这条消息
            if (topic.empty())
            {
                continue;
            }

            // 查找对应的发布者
            auto publisherIt = m_publishers.find(topic);
            if (publisherIt != m_publishers.end())
            {
                auto msg_str = as_string_view(mcapMessage.data, mcapMessage.dataSize);

                publisherIt->second->Publish(std::string(msg_str));

                // 增加已播放消息计数
                m_playedMessages++;
            }
        }

        // 是否需要循环播放
        if (m_running && m_config.loop_playback)
        {
            // 重置播放状态并重新开始
            m_playedMessages = 0;
            // 递归调用实现循环播放
            PlayLoop();
        } else
        {
            // 完成播放
            m_state = PlayerState::STOPPED;
        }
    }
    inline std::string_view as_string_view(const std::byte* data, size_t size) { return {reinterpret_cast<const char*>(data), size}; }

private:
    PlayerConfig m_config;                                              ///< 配置
    ReaderPtr m_mcapReader;                                             ///< MCAP读取器
    std::unordered_map<std::string, OpenbagPublisherPtr> m_publishers;  ///< 发布者映射
    MessageAdapterFactoryPtr m_adapterFactory;                          ///< 消息适配器工厂
    PublisherFunc m_publisherFunc;                                      ///< 发布者函数

    std::atomic<PlayerState> m_state;        ///< 播放状态
    std::atomic<bool> m_running;             ///< 线程运行标志
    std::atomic<uint64_t> m_playedMessages;  ///< 已播放消息数
    std::thread m_playThread;                ///< 播放线程
    std::mutex m_mutex;                      ///< 互斥锁
    std::condition_variable m_playPauseCV;   ///< 播放/暂停条件变量
};

}  // namespace openbag
