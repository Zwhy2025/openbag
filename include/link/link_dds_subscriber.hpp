#pragma once

#include "link/link.hpp" // For Link::SubscriberBase
#include "examples/message/generated/general.h"
#include "examples/message/generated/generalPubSubTypes.h"
#include "examples/message/generated/test.pb.h" // For Test message

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/rtps/common/types.h> // For ReturnCode_t

#include <string>
#include <functional> // For std::function
#include <google/protobuf/message.h>
#include <type_traits> // For std::is_base_of, std::is_same
#include <iostream>    // For std::cout, std::cerr (commented out)
#include <stdexcept>   // For std::runtime_error

namespace link {

template <typename T>
class DdsSubscriber : public SubscriberBase<T> {
public:
    using UserCallbackType = std::function<void(const T&)>;

    // --- DdsSubscriberListener Inner Class ---
    class DdsSubscriberListener : public eprosima::fastdds::dds::DataReaderListener {
    public:
        DdsSubscriberListener(DdsSubscriber<T>* owner_subscriber, UserCallbackType user_callback)
            : owner_subscriber_(owner_subscriber), user_callback_(user_callback) {}

        void on_data_available(eprosima::fastdds::dds::DataReader* reader) override {
            eprosima::fastdds::dds::SampleInfo info;
            // The received_general_msg_ is a member to reuse its memory.
            if (reader->read_next_sample(&received_general_msg_, &info) == eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK) {
                if (info.instance_state == eprosima::fastdds::dds::ALIVE_INSTANCE_STATE) {
                    // std::cout << "Listener: Sample received on topic " << reader->get_topicdescription()->get_name() << std::endl;
                    deserialize_and_invoke(received_general_msg_);
                }
            } else {
                // std::cerr << "Listener ERROR: Failed to read sample on topic " << reader->get_topicdescription()->get_name() << std::endl;
            }
        }

        void on_subscription_matched(
                eprosima::fastdds::dds::DataReader* reader,
                const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override {
            if (info.current_count_change == 1) {
                // std::cout << "Listener: Matched publisher for topic: " << reader->get_topicdescription()->get_name() << std::endl;
            } else if (info.current_count_change == -1) {
                // std::cout << "Listener: Unmatched publisher for topic: " << reader->get_topicdescription()->get_name() << std::endl;
            } else {
                // std::cout << "Listener: " << info.current_count_change
                        //   << " is not a valid value for SubscriptionMatchedStatus current count change for topic: "
                        //   << reader->get_topicdescription()->get_name() << std::endl;
            }
        }

        void on_requested_deadline_missed(
            eprosima::fastdds::dds::DataReader* reader,
            const eprosima::fastdds::dds::RequestedDeadlineMissedStatus& status) override {
            // std::cerr << "Listener WARN: Requested deadline missed on topic: " << reader->get_topicdescription()->get_name()
                    //   << " Total count: " << status.total_count() << " Change: " << status.total_count_change() << std::endl;
        }

    private:
        // Deserialization helper for Protobuf
        template <typename U = T,
                  typename std::enable_if<std::is_base_of<google::protobuf::Message, U>::value, int>::type = 0>
        bool deserialize_and_invoke(const general::GeneralMessage& general_msg) {
            static_assert(std::is_same<T, U>::value, "Type mismatch in Protobuf deserialize specialization.");
            if (general_msg.type() == "proto") {
                T specific_message;
                if (specific_message.ParseFromArray(general_msg.data().data(), static_cast<int>(general_msg.data().size()))) {
                    if (user_callback_) {
                        user_callback_(specific_message);
                        return true;
                    }
                } else {
                    // std::cerr << "Listener ERROR: Failed to deserialize Protobuf message." << std::endl;
                }
            } else {
                // std::cerr << "Listener WARN: Received message type '" << general_msg.type()
                        //   << "' but expected 'proto' for Protobuf deserialization." << std::endl;
            }
            return false;
        }

        // Deserialization helper for std::string
        template <typename U = T,
                  typename std::enable_if<std::is_same<U, std::string>::value, int>::type = 0>
        bool deserialize_and_invoke(const general::GeneralMessage& general_msg) {
            static_assert(std::is_same<T, U>::value, "Type mismatch in std::string deserialize specialization.");
            if (general_msg.type() == "string") {
                U received_string(general_msg.data().begin(), general_msg.data().end());
                if (user_callback_) {
                    user_callback_(received_string);
                    return true;
                }
            } else {
                // std::cerr << "Listener WARN: Received message type '" << general_msg.type()
                        //   << "' but expected 'string' for std::string deserialization." << std::endl;
            }
            return false;
        }

        DdsSubscriber<T>* owner_subscriber_;
        UserCallbackType user_callback_;
        general::GeneralMessage received_general_msg_;
    }; // End DdsSubscriberListener

    // --- DdsSubscriber<T> Constructor ---
    DdsSubscriber(const std::string& topic_name,
                  eprosima::fastdds::dds::DomainParticipant* participant,
                  UserCallbackType callback)
        : topic_name_(topic_name),
          participant_(participant),
          dds_subscriber_(nullptr),
          topic_(nullptr),
          reader_(nullptr),
          type_support_(new general::GeneralMessagePubSubType()), // From generalPubSubTypes.h
          listener_(this, callback), // Pass self and callback to listener
          user_callback_(callback) { // Store callback
        if (!participant_) {
            throw std::runtime_error("DdsSubscriber: DomainParticipant is null for topic " + topic_name_ + "!");
        }
        if (!user_callback_) {
             throw std::runtime_error("DdsSubscriber: User callback is null for topic " + topic_name_ + "!");
        }

        type_support_.register_type(participant_);

        dds_subscriber_ = participant_->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT, nullptr);
        if (dds_subscriber_ == nullptr) {
            throw std::runtime_error("DdsSubscriber: Failed to create DDS Subscriber for topic " + topic_name_);
        }

        topic_ = participant_->create_topic(
            topic_name_,
            type_support_.get_type_name(),
            eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) {
            participant_->delete_subscriber(dds_subscriber_);
            dds_subscriber_ = nullptr;
            throw std::runtime_error("DdsSubscriber: Failed to create DDS Topic " + topic_name_);
        }

        eprosima::fastdds::dds::DataReaderQos rqos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
        rqos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        rqos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        rqos.history().depth = 10;

        reader_ = dds_subscriber_->create_datareader(topic_, rqos, &listener_);
        if (reader_ == nullptr) {
            participant_->delete_topic(topic_);
            topic_ = nullptr;
            participant_->delete_subscriber(dds_subscriber_);
            dds_subscriber_ = nullptr;
            throw std::runtime_error("DdsSubscriber: Failed to create DDS DataReader for topic " + topic_name_);
        }
        // std::cout << "DdsSubscriber: Successfully created for topic: " << topic_name_ << std::endl;
    }

    // --- DdsSubscriber<T> Destructor ---
    ~DdsSubscriber() override {
        if (reader_ != nullptr && dds_subscriber_ != nullptr) {
            dds_subscriber_->delete_datareader(reader_);
            // reader_ = nullptr; // Owned by dds_subscriber_
        }
        if (topic_ != nullptr && participant_ != nullptr) {
            participant_->delete_topic(topic_);
            // topic_ = nullptr; // Owned by participant_
        }
        if (dds_subscriber_ != nullptr && participant_ != nullptr) {
            participant_->delete_subscriber(dds_subscriber_);
            // dds_subscriber_ = nullptr; // Owned by participant_
        }
        // std::cout << "DdsSubscriber: Destroyed for topic: " << topic_name_ << std::endl;
    }

    const std::string& get_topic_name() const override { return topic_name_; }

private:
    // --- Member Variables ---
    std::string topic_name_;
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Subscriber* dds_subscriber_;
    eprosima::fastdds::dds::Topic* topic_;
    eprosima::fastdds::dds::DataReader* reader_;
    eprosima::fastdds::dds::TypeSupport type_support_; // Manages GeneralMessagePubSubType
    DdsSubscriberListener listener_;
    UserCallbackType user_callback_; // Stored primarily for the listener
};

} // namespace link
