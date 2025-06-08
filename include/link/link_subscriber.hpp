/**
 * @author Zhao Jun (zwhy2025@gmail.com)
 * @version 0.1
 * @date 2024-07-30
 *
 * @file link_subscriber.hpp
 * @brief Link库的DDS订阅者实现
 *
 * 本文件提供了基于FastDDS的通用订阅者模板类`DDSSubscriber`，
 * 支持订阅Protobuf消息和`std::string`类型的数据。
 *
 * 主要类与方法
 * - SubscriberBase: 订阅者基类接口
 * - DDSSubscriber: FastDDS订阅者实现类
 *   - GetTopicName: 获取主题名称
 * - CreateSubscriber: 创建订阅者实例的工厂函数
 */

#pragma once

#include <fastdds/rtps/common/Types.h>
#include <google/protobuf/message.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "dds_participant.hpp"
#include "general.h"
#include "generalPubSubTypes.h"

namespace Link {
/**
 * @brief 订阅者基类接口，定义了订阅消息的通用契约。
 */
template <typename T>
class SubscriberBase
{
public:
    virtual ~SubscriberBase() = default;
    /**
     * @brief 获取当前订阅者关联的主题名称
     * @return 主题名称的常量引用
     */
    virtual const std::string& GetTopicName() const = 0;
};

/**
 * @brief 基于FastDDS的通用订阅者实现类，支持Protobuf和std::string类型。
 * @tparam T 消息类型，可以是Protobuf消息或std::string
 */
template <typename T>
class DDSSubscriber : public SubscriberBase<T>
{
public:
    /**
     * @brief 用户回调函数的类型定义，用于处理接收到的消息。
     */
    using UserCallbackType = std::function<void(const T&)>;

    /**
     * @brief DDSSubscriber的监听器，处理FastDDS的数据可用、订阅匹配和请求截止日期丢失事件。
     */
    class DDSSubscriberListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:
        /**
         * @brief 构造函数，初始化DDSSubscriberListener实例。
         * @param owner_subscriber 拥有此监听器的DDSSubscriber实例指针
         * @param user_callback 用户提供的消息处理回调函数
         */
        DDSSubscriberListener(DDSSubscriber<T>* owner_subscriber, UserCallbackType user_callback) : m_ownerSubscriber(owner_subscriber), m_userCallback(user_callback) {}

        /**
         * @brief 当数据可用时调用的回调函数。
         * @param reader 发生数据可用事件的数据读取器
         */
        void on_data_available(eprosima::fastdds::dds::DataReader* reader) override
        {
            eprosima::fastdds::dds::SampleInfo info;
            General::Message receivedGeneralMsgLocal;  // Declare a local message object
            if (reader->read_next_sample(&receivedGeneralMsgLocal, &info) == eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
            {
                if (info.instance_state == eprosima::fastdds::dds::ALIVE_INSTANCE_STATE)
                {
                    DeserializeAndInvoke(receivedGeneralMsgLocal);
                }
            }
        }

        /**
         * @brief 当订阅者与发布者匹配时调用的回调函数。
         * @param reader 发生匹配事件的数据读取器
         * @param info 匹配状态信息
         */
        void on_subscription_matched(eprosima::fastdds::dds::DataReader* reader, const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override {}

        /**
         * @brief 当请求的截止日期错过时调用的回调函数。
         * @param reader 发生截止日期错过事件的数据读取器
         * @param status 截止日期错过状态信息
         */
        void on_requested_deadline_missed(eprosima::fastdds::dds::DataReader* reader, const eprosima::fastdds::dds::RequestedDeadlineMissedStatus& status) override {}

    private:
        /**
         * @brief 反序列化Protobuf消息并调用用户回调函数。
         * @tparam U 消息类型，必须是google::protobuf::Message的派生类
         * @param general_msg 包含序列化数据的通用消息
         * @return true表示成功反序列化并调用回调，false表示失败
         */
        template <typename U = T, typename std::enable_if<std::is_base_of<google::protobuf::Message, U>::value, int>::type = 0>
        bool DeserializeAndInvoke(const General::Message& general_msg)
        {
            static_assert(std::is_same<T, U>::value, "Type mismatch in Protobuf deserialize specialization.");
            if (general_msg.header().type() == "proto")
            {
                T specificMessage;
                if (specificMessage.ParseFromArray(general_msg.payload().data(), static_cast<int>(general_msg.payload().size())))
                {
                    if (m_userCallback)
                    {
                        m_userCallback(specificMessage);
                        return true;
                    }
                }
            }
            return false;
        }

        /**
         * @brief 反序列化std::string消息并调用用户回调函数。
         * @tparam U 消息类型，必须是std::string
         * @param general_msg 包含序列化数据的通用消息
         * @return true表示成功反序列化并调用回调，false表示失败
         */
        template <typename U = T, typename std::enable_if<std::is_same<U, std::string>::value, int>::type = 0>
        bool DeserializeAndInvoke(const General::Message& general_msg)
        {
            static_assert(std::is_same<T, U>::value, "Type mismatch in std::string deserialize specialization.");
            if (general_msg.header().type() == "string")
            {
                U receivedString(general_msg.payload().begin(), general_msg.payload().end());
                if (m_userCallback)
                {
                    m_userCallback(receivedString);
                    return true;
                }
            }
            return false;
        }

        DDSSubscriber<T>* m_ownerSubscriber;  ///< 拥有此监听器的DDSSubscriber实例指针
        UserCallbackType m_userCallback;      ///< 用户提供的消息处理回调函数
    };

    /**
     * @brief 构造函数，初始化DDSSubscriber实例。
     * @param topic_name 要订阅的主题名称
     * @param callback 用户提供的消息处理回调函数
     * @exception std::runtime_error 如果DomainParticipant为null或用户回调为null或创建DDS实体失败
     */
    DDSSubscriber(const std::string& topic_name, UserCallbackType callback)
        : m_topicName(topic_name),
          m_participant(Link::Participant::GetParticipant()),
          m_ddsSubscriber(nullptr),
          m_topic(nullptr),
          m_reader(nullptr),
          m_typeSupport(new General::MessagePubSubType()),
          m_listener(this, callback),
          m_userCallback(callback)
    {
        if (!m_participant)
        {
            throw std::runtime_error("DdsSubscriber: DomainParticipant is null for topic " + m_topicName + "!");
        }
        if (!m_userCallback)
        {
            throw std::runtime_error("DdsSubscriber: User callback is null for topic " + m_topicName + "!");
        }

        m_typeSupport.register_type(m_participant);

        m_ddsSubscriber = m_participant->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT, nullptr);
        if (m_ddsSubscriber == nullptr)
        {
            throw std::runtime_error("DdsSubscriber: Failed to create DDS Subscriber for topic " + m_topicName);
        }

        m_topic = m_participant->create_topic(m_topicName, m_typeSupport.get_type_name(), eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        if (m_topic == nullptr)
        {
            m_participant->delete_subscriber(m_ddsSubscriber);
            m_ddsSubscriber = nullptr;
            throw std::runtime_error("DdsSubscriber: Failed to create DDS Topic " + m_topicName);
        }

        eprosima::fastdds::dds::DataReaderQos rqos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
        rqos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        rqos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        rqos.history().depth = 10;

        m_reader = m_ddsSubscriber->create_datareader(m_topic, rqos, &m_listener);
        if (m_reader == nullptr)
        {
            m_participant->delete_topic(m_topic);
            m_topic = nullptr;
            m_participant->delete_subscriber(m_ddsSubscriber);
            m_ddsSubscriber = nullptr;
            throw std::runtime_error("DdsSubscriber: Failed to create DDS DataReader for topic " + m_topicName);
        }
    }

    /**
     * @brief 析构函数，清理FastDDS相关的资源。
     */
    ~DDSSubscriber() override
    {
        if (m_reader != nullptr && m_ddsSubscriber != nullptr)
        {
            m_ddsSubscriber->delete_datareader(m_reader);
        }
        if (m_topic != nullptr && m_participant != nullptr)
        {
            m_participant->delete_topic(m_topic);
        }
        if (m_ddsSubscriber != nullptr && m_participant != nullptr)
        {
            m_participant->delete_subscriber(m_ddsSubscriber);
        }
    }

    /**
     * @brief 获取当前订阅者关联的主题名称。
     * @return 主题名称的常量引用。
     */
    const std::string& GetTopicName() const override { return m_topicName; }

private:
    std::string m_topicName;                                   ///< 用于存储主题名称
    eprosima::fastdds::dds::DomainParticipant* m_participant;  ///< FastDDS域参与者
    eprosima::fastdds::dds::Subscriber* m_ddsSubscriber;       ///< FastDDS订阅者
    eprosima::fastdds::dds::Topic* m_topic;                    ///< FastDDS主题
    eprosima::fastdds::dds::DataReader* m_reader;              ///< FastDDS数据读取器
    eprosima::fastdds::dds::TypeSupport m_typeSupport;         ///< FastDDS类型支持
    DDSSubscriberListener m_listener;                          ///< 订阅者监听器
    UserCallbackType m_userCallback;                           ///< 用户提供的消息处理回调函数
};

/**
 * @brief 创建Link::SubscriberBase<T>的共享指针实例的工厂函数。
 * @tparam T 消息类型
 * @param topic_name 要订阅的主题名称
 * @param callback 用户提供的消息处理回调函数
 * @return Link::SubscriberBase<T>的共享指针
 */
template <typename T>
std::shared_ptr<Link::SubscriberBase<T>> CreateSubscriber(const std::string& topic_name, std::function<void(const T&)> callback)
{
    return std::make_shared<DDSSubscriber<T>>(topic_name, callback);
}

}  // namespace Link
