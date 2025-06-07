/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file link_transport.hpp
 * @brief Link transport adapter
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "link/link.hpp"
#include "openbag/transport.hpp"

/**
 * @brief Link订阅者适配器，统一实现不同类型的订阅
 */
class LinkSubscriberAdapter : public ::openbag::OpenbagSubscriberBase
{
public:
    /**
     * @brief 构造函数 - 字符串类型
     * @param topic 话题名称
     * @param callback 字符串回调函数
     */
    LinkSubscriberAdapter(const std::string& topic, std::function<void(const std::string&)> callback) : topic_name_(topic)
    {
        // 使用link库创建字符串订阅者
        link_subscriber_ = Link::CreateSubscriber<std::string>(topic, callback);
    }

    /**
     * @brief 析构函数
     */
    ~LinkSubscriberAdapter() override = default;

    /**
     * @brief 获取订阅的话题名称
     * @return 话题名称
     */
    std::string GetTopicName() const override { return topic_name_; }

private:
    std::string topic_name_;
    std::shared_ptr<Link::SubscriberBase> link_subscriber_;
};

/**
 * @brief Link发布者适配器
 */
class LinkPublisherAdapter : public ::openbag::OpenbagPublisherBase
{
public:
    /**
     * @brief 构造函数
     * @param topic 话题名称
     */
    explicit LinkPublisherAdapter(const std::string& topic) : topic_name_(topic)
    {
        // 使用link库创建字符串发布者
        link_publisher_ = Link::CreatePublisher<std::string>(topic);
    }

    /**
     * @brief 析构函数
     */
    ~LinkPublisherAdapter() override = default;

    /**
     * @brief 获取发布的话题名称
     * @return 话题名称
     */
    std::string GetTopicName() const override { return topic_name_; }

    /**
     * @brief 发布消息
     * @param data 消息数据
     * @return 是否发布成功
     */
    bool Publish(const std::string& data) override
    {
        if (link_publisher_)
        {
            return link_publisher_->Publish(data);
        }

    }  // namespace openbag       return false;
}

private : std::string topic_name_;
std::shared_ptr<Link::PublisherBase<std::string>> link_publisher_;
}
;

/**
 * @brief Link适配器工厂
 */
class LinkAdapterFactory : public ::openbag::MessageAdapterFactory
{
public:
    /**
     * @brief 构造函数
     */
    LinkAdapterFactory() = default;

    /**
     * @brief 析构函数
     */
    ~LinkAdapterFactory() override = default;

    /**
     * @brief 创建发布者
     * @param topic 话题名称
     * @return 发布者基类指针
     */
    std::shared_ptr<::openbag::OpenbagPublisherBase> CreatePublisher(const std::string& topic) override { return std::make_shared<LinkPublisherAdapter>(topic); }

    /**
     * @brief 获取单例实例
     * @return 工厂实例指针
     */
    static std::shared_ptr<LinkAdapterFactory> GetInstance()
    {
        static auto instance = std::make_shared<LinkAdapterFactory>();
        return instance;
    }
};

/**
 * @brief 便捷函数：获取Link消息适配器工厂
 * @return 消息适配器工厂指针
 */
inline ::openbag::MessageAdapterFactoryPtr GetLinkAdapterFactory() { return LinkAdapterFactory::GetInstance(); }

// 模板特化实现 - 在头文件中提供
namespace openbag {

template <>
inline std::shared_ptr<OpenbagSubscriberBase> MessageAdapterFactory::CreateSubscriberInternal<std::string>(const std::string& topic,
                                                                                                           std::function<void(const std::string&)> callback)
{
    return std::make_shared<LinkSubscriberAdapter>(topic, callback);
}

}  // namespace openbag