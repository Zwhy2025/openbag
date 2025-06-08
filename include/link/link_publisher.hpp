/**
 * @author Zhao Jun (zwhy2025@gmail.com)
 * @version 0.1
 * @date 2024-07-30
 *
 * @file link_publisher.hpp
 * @brief Link库的DDS发布者实现
 *
 * 本文件提供了基于FastDDS的通用发布者模板类`DDSPublisher`，
 * 支持发布Protobuf消息和`std::string`类型的数据。
 *
 * 主要类与方法
 * - PublisherBase: 发布者基类接口
 * - DDSPublisher: FastDDS发布者实现类
 *   - Publish: 发布消息
 *   - GetTopicName: 获取主题名称
 * - CreatePublisher: 创建发布者实例的工厂函数
 */

#pragma once

#include <google/protobuf/message.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "dds_participant.hpp"
#include "general.h"
#include "generalPubSubTypes.h"
#include "test.pb.h"

namespace Link {
/**
 * @brief 发布者基类接口，定义了发布消息的通用契约。
 */
template <typename T>
class PublisherBase
{
public:
    virtual ~PublisherBase() = default;
    /**
     * @brief 发布消息
     * @param message 待发布的消息
     * @return true表示发布成功，false表示发布失败
     */
    virtual bool Publish(const T& message) = 0;
    /**
     * @brief 获取当前发布者关联的主题名称
     * @return 主题名称的常量引用
     */
    virtual const std::string& GetTopicName() const = 0;
};

/**
 * @brief 基于FastDDS的通用发布者实现类，支持Protobuf和std::string类型。
 * @tparam T 消息类型，可以是Protobuf消息或std::string
 */
template <typename T>
class DDSPublisher : public PublisherBase<T>
{
public:
    /**
     * @brief DDSPublisher的监听器，处理FastDDS的发布匹配事件。
     */
    class DDSPublisherListener : public eprosima::fastdds::dds::DataWriterListener
    {
    public:
        DDSPublisherListener() = default;
        ~DDSPublisherListener() override = default;

        /**
         * @brief 当发布者与订阅者匹配时调用的回调函数。
         * @param writer 发生匹配事件的数据写入器
         * @param info 匹配状态信息
         */
        void on_publication_matched(eprosima::fastdds::dds::DataWriter* writer, const eprosima::fastdds::dds::PublicationMatchedStatus& info) override {}
    };

    /**
     * @brief 构造函数，初始化DDSPublisher实例。
     * @param topic_name 要发布的主题名称
     * @exception std::runtime_error 如果DomainParticipant为null或创建DDS实体失败
     */
    DDSPublisher(const std::string& topic_name)
        : m_topicName(topic_name),
          m_participant(Link::Participant::get_participant()),
          m_ddsPublisher(nullptr),
          m_topic(nullptr),
          m_writer(nullptr),
          m_typeSupport(new General::MessagePubSubType())
    {
        if (!m_participant)
        {
            throw std::runtime_error("DdsPublisher: DomainParticipant is null for topic " + m_topicName + "!");
        }

        m_typeSupport.register_type(m_participant);

        m_ddsPublisher = m_participant->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT, nullptr);
        if (m_ddsPublisher == nullptr)
        {
            throw std::runtime_error("DdsPublisher: Failed to create DDS Publisher for topic " + m_topicName);
        }

        m_topic = m_participant->create_topic(m_topicName, m_typeSupport.get_type_name(), eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        if (m_topic == nullptr)
        {
            m_participant->delete_publisher(m_ddsPublisher);
            m_ddsPublisher = nullptr;
            throw std::runtime_error("DdsPublisher: Failed to create DDS Topic " + m_topicName);
        }

        eprosima::fastdds::dds::DataWriterQos wqos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
        wqos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        wqos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        wqos.history().depth = 10;

        m_writer = m_ddsPublisher->create_datawriter(m_topic, wqos, &m_listener);
        if (m_writer == nullptr)
        {
            m_participant->delete_topic(m_topic);
            m_topic = nullptr;
            m_participant->delete_publisher(m_ddsPublisher);
            m_ddsPublisher = nullptr;
            throw std::runtime_error("DdsPublisher: Failed to create DDS DataWriter for topic " + m_topicName);
        }
    }

    /**
     * @brief 析构函数，清理FastDDS相关的资源。
     */
    ~DDSPublisher() override
    {
        if (m_writer != nullptr && m_ddsPublisher != nullptr)
        {
            m_ddsPublisher->delete_datawriter(m_writer);
        }
        if (m_topic != nullptr && m_participant != nullptr)
        {
            m_participant->delete_topic(m_topic);
        }
        if (m_ddsPublisher != nullptr && m_participant != nullptr)
        {
            m_participant->delete_publisher(m_ddsPublisher);
        }
    }

    /**
     * @brief 发布消息。
     * @param message 待发布的消息
     * @return true表示发布成功，false表示发布失败（例如DataWriter未初始化）
     */
    bool Publish(const T& message) override
    {
        if (m_writer == nullptr)
        {
            return false;
        }
        if constexpr (std::is_same<T, std::string>::value)
        {
            return TransferAndPublish(message);
        } else if constexpr (std::is_base_of<google::protobuf::Message, T>::value)
        {
            return SerializeAndPublish(message);
        }
        return false;
    }

    /**
     * @brief 获取当前发布者关联的主题名称。
     * @return 主题名称的常量引用。
     */
    const std::string& GetTopicName() const override { return m_topicName; }

private:
    bool TransferAndPublish(const std::string& message)
    {
        m_generalMsgInstance.header().type("string");
        m_generalMsgInstance.payload().assign(message.begin(), message.end());
        return m_writer->write(&m_generalMsgInstance);
    }

    /**
     * @brief 序列化Protobuf消息并发布。
     * @tparam U 消息类型，必须是google::protobuf::Message的派生类
     * @param proto_message 待序列化的Protobuf消息
     * @return true表示成功序列化并发布，false表示失败
     */
    template <typename U = T, typename std::enable_if<std::is_base_of<google::protobuf::Message, U>::value, int>::type = 0>
    bool SerializeAndPublish(const U& proto_message)
    {
        static_assert(std::is_same<T, U>::value, "Type mismatch in Protobuf publish specialization.");
        m_generalMsgInstance.header().type("proto");
        std::string serializedData;
        if (!proto_message.SerializeToString(&serializedData))
        {
            return false;
        }
        m_generalMsgInstance.payload().assign(serializedData.begin(), serializedData.end());
        return m_writer->write(&m_generalMsgInstance);
    }

    /**
     * @brief 序列化std::string消息并发布。
     * @tparam U 消息类型，必须是std::string
     * @param string_message 待序列化的std::string消息
     * @return true表示成功序列化并发布，false表示失败
     */
    template <typename U = T, typename std::enable_if<std::is_same<U, std::string>::value, int>::type = 0>
    bool SerializeAndPublish(const U& string_message)
    {
        static_assert(std::is_same<T, U>::value, "Type mismatch in std::string publish specialization.");
        m_generalMsgInstance.header().type("string");
        m_generalMsgInstance.payload().assign(string_message.begin(), string_message.end());
        return m_writer->write(&m_generalMsgInstance);
    }

    std::string m_topicName;                                   ///< 用于存储主题名称
    eprosima::fastdds::dds::DomainParticipant* m_participant;  ///< FastDDS域参与者
    eprosima::fastdds::dds::Publisher* m_ddsPublisher;         ///< FastDDS发布者
    eprosima::fastdds::dds::Topic* m_topic;                    ///< FastDDS主题
    eprosima::fastdds::dds::DataWriter* m_writer;              ///< FastDDS数据写入器
    eprosima::fastdds::dds::TypeSupport m_typeSupport;         ///< FastDDS类型支持
    General::Message m_generalMsgInstance;                     ///< 通用消息实例，用于序列化和发布
    DDSPublisherListener m_listener;                           ///< 发布者监听器
};

/**
 * @brief 创建Link::PublisherBase<T>的共享指针实例的工厂函数。
 * @tparam T 消息类型
 * @param topic_name 要发布的主题名称
 * @return Link::PublisherBase<T>的共享指针
 */
template <typename T>
std::shared_ptr<Link::PublisherBase<T>> CreatePublisher(const std::string& topic_name)
{
    return std::make_shared<DDSPublisher<T>>(topic_name);
}

}  // namespace Link
