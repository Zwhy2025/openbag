/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file message_adapter.hpp
 * @brief 消息适配器接口，用于抽象化消息发布订阅
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

namespace openbag {

/**
 * @brief 消息类型特性检查命名空间，避免重复定义
 */
namespace type_traits {

/**
 * @brief 类型特性检查：是否具有ParseFromString方法（如Protobuf）
 */
template <typename T, typename = void>
struct has_parse_from_string : std::false_type
{
};

template <typename T>
struct has_parse_from_string<
    T, std::void_t<decltype(std::declval<T>().ParseFromString(std::string()))>> : std::true_type
{
};

/**
 * @brief 类型特性检查：是否具有SerializeToString方法（如Protobuf）
 */
template <typename T, typename = void>
struct has_serialize_to_string : std::false_type
{
};

template <typename T>
struct has_serialize_to_string<
    T, std::void_t<decltype(std::declval<T>().SerializeToString(std::declval<std::string*>()))>>
    : std::true_type
{
};

}  // namespace type_traits

/**
 * @brief 订阅器基类接口
 */
class OpenbagSubscriberBase
{
public:
    virtual ~OpenbagSubscriberBase() = default;

    /**
     * @brief 获取订阅的话题名称
     * @return 话题名称
     */
    virtual std::string GetTopicName() const = 0;
};

/**
 * @brief 发布器基类接口
 */
class OpenbagPublisherBase
{
public:
    virtual ~OpenbagPublisherBase() = default;

    /**
     * @brief 获取发布的话题名称
     * @return 话题名称
     */
    virtual std::string GetTopicName() const = 0;

    /**
     * @brief 发布消息（统一使用字符串接口）
     * @param data 消息数据
     * @return 是否发布成功
     */
    virtual bool Publish(const std::string& data) = 0;
};

/**
 * @brief 消息适配器工厂接口
 * 提供模板方法，让子类实现具体的创建逻辑
 */
class MessageAdapterFactory
{
public:
    virtual ~MessageAdapterFactory() = default;

    /**
     * @brief 创建订阅者（模板方法）
     * @tparam T 消息类型
     * @param topic 话题名称
     * @param callback 类型化回调函数
     * @return 订阅者基类指针
     */
    template <typename T>
    std::shared_ptr<OpenbagSubscriberBase> CreateSubscriber(const std::string& topic,
                                                            std::function<void(const T&)> callback)
    {
        return CreateSubscriberInternal<T>(topic, callback);
    }

    /**
     * @brief 创建发布者
     * @param topic 话题名称
     * @return 发布者基类指针
     */
    virtual std::shared_ptr<OpenbagPublisherBase> CreatePublisher(const std::string& topic) = 0;

protected:
    /**
     * @brief 内部创建订阅者方法 - 由子类实现
     * @tparam T 消息类型
     * @param topic 话题名称
     * @param callback 类型化回调函数
     * @return 订阅者基类指针
     */
    template <typename T>
    std::shared_ptr<OpenbagSubscriberBase> CreateSubscriberInternal(
        const std::string& topic, std::function<void(const T&)> callback);
};

using MessageAdapterFactoryPtr = std::shared_ptr<MessageAdapterFactory>;
using OpenbagSubscriberPtr = std::shared_ptr<OpenbagSubscriberBase>;
using OpenbagPublisherPtr = std::shared_ptr<OpenbagPublisherBase>;
}  // namespace openbag