#pragma once

#include "link/link.hpp" // For Link::PublisherBase
#include "examples/message/generated/general.h"
#include "examples/message/generated/generalPubSubTypes.h"
#include "examples/message/generated/test.pb.h" // For Test message

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

#include <string>
#include <vector>
#include <google/protobuf/message.h>
#include <type_traits> // For std::is_base_of, std::is_same
#include <iostream>    // For std::cout, std::cerr (commented out)
#include <stdexcept>   // For std::runtime_error

namespace link {

// Forward declare DdsSubscriber (already in original hpp)
template <typename T> class DdsSubscriber;

template <typename T>
class DdsPublisher : public PublisherBase<T> {
public:
    // --- DdsPublisherListener Inner Class ---
    class DdsPublisherListener : public eprosima::fastdds::dds::DataWriterListener {
    public:
        DdsPublisherListener() = default; // Explicitly default constructor
        ~DdsPublisherListener() override = default; // Explicitly default destructor

        void on_publication_matched(
                eprosima::fastdds::dds::DataWriter* writer,
                const eprosima::fastdds::dds::PublicationMatchedStatus& info) override {
            if (info.current_count_change == 1) {
                // std::cout << "DdsPublisher: Matched subscriber for topic: " << writer->get_topic()->get_name() << std::endl;
            } else if (info.current_count_change == -1) {
                // std::cout << "DdsPublisher: Unmatched subscriber for topic: " << writer->get_topic()->get_name() << std::endl;
            } else {
                // std::cout << "DdsPublisher: " << info.current_count_change
                        //   << " is not a valid value for PublicationMatchedStatus current count change for topic: " << writer->get_topic()->get_name() << std::endl;
            }
        }
    }; // End DdsPublisherListener

    // --- DdsPublisher<T> Constructor ---
    DdsPublisher(const std::string& topic_name, eprosima::fastdds::dds::DomainParticipant* participant)
        : topic_name_(topic_name),
          participant_(participant),
          dds_publisher_(nullptr),
          topic_(nullptr),
          writer_(nullptr),
          type_support_(new general::GeneralMessagePubSubType()) { // Initialize TypeSupport with the concrete PubSubType
        if (!participant_) {
            throw std::runtime_error("DdsPublisher: DomainParticipant is null for topic " + topic_name_ + "!");
        }

        type_support_.register_type(participant_);

        dds_publisher_ = participant_->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT, nullptr);
        if (dds_publisher_ == nullptr) {
            throw std::runtime_error("DdsPublisher: Failed to create DDS Publisher for topic " + topic_name_);
        }

        topic_ = participant_->create_topic(
            topic_name_,
            type_support_.get_type_name(),
            eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) {
            participant_->delete_publisher(dds_publisher_);
            dds_publisher_ = nullptr;
            throw std::runtime_error("DdsPublisher: Failed to create DDS Topic " + topic_name_);
        }

        eprosima::fastdds::dds::DataWriterQos wqos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
        wqos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        wqos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        wqos.history().depth = 10;

        writer_ = dds_publisher_->create_datawriter(topic_, wqos, &listener_);
        if (writer_ == nullptr) {
            participant_->delete_topic(topic_);
            topic_ = nullptr;
            participant_->delete_publisher(dds_publisher_);
            dds_publisher_ = nullptr;
            throw std::runtime_error("DdsPublisher: Failed to create DDS DataWriter for topic " + topic_name_);
        }
        // std::cout << "DdsPublisher: Successfully created for topic: " << topic_name_ << std::endl;
    }

    // --- DdsPublisher<T> Destructor ---
    ~DdsPublisher() override {
        if (writer_ != nullptr && dds_publisher_ != nullptr) {
            dds_publisher_->delete_datawriter(writer_);
            // writer_ = nullptr; // DataWriter is owned by Publisher
        }
        if (topic_ != nullptr && participant_ != nullptr) {
            participant_->delete_topic(topic_);
            // topic_ = nullptr; // Topic is owned by DomainParticipant
        }
        if (dds_publisher_ != nullptr && participant_ != nullptr) {
            participant_->delete_publisher(dds_publisher_);
            // dds_publisher_ = nullptr; // Publisher is owned by DomainParticipant
        }
        // std::cout << "DdsPublisher: Destroyed for topic: " << topic_name_ << std::endl;
    }

    // --- DdsPublisher<T> publish Method ---
    bool publish(const T& message) override {
        if (writer_ == nullptr) {
            // std::cerr << "DdsPublisher ERROR: DataWriter is null for topic " << topic_name_ << ". Cannot publish." << std::endl;
            return false;
        }
        return serialize_and_publish(message);
    }

    const std::string& get_topic_name() const override { return topic_name_; }

private:
    // --- SFINAE Helper Implementations ---
    // Overload for google::protobuf::Message
    template <typename U = T, // U defaults to T
              typename std::enable_if<std::is_base_of<google::protobuf::Message, U>::value, int>::type = 0>
    bool serialize_and_publish(const U& proto_message) {
        static_assert(std::is_same<T, U>::value, "Type mismatch in Protobuf publish specialization.");
        general_msg_instance_.type("proto"); // Set type for GeneralMessage
        std::string serialized_data;
        if (!proto_message.SerializeToString(&serialized_data)) {
            // std::cerr << "DdsPublisher ERROR: Failed to serialize Protobuf message for topic " << topic_name_ << std::endl;
            return false;
        }
        // Fast DDS expects std::vector<unsigned char> for octet sequence.
        // general::GeneralMessage's data field should be std::vector<uint8_t> or similar.
        general_msg_instance_.data().assign(serialized_data.begin(), serialized_data.end());
        return writer_->write(&general_msg_instance_);
    }

    // Overload for std::string
    template <typename U = T, // U defaults to T
              typename std::enable_if<std::is_same<U, std::string>::value, int>::type = 0>
    bool serialize_and_publish(const U& string_message) {
        static_assert(std::is_same<T, U>::value, "Type mismatch in std::string publish specialization.");
        general_msg_instance_.type("string"); // Set type for GeneralMessage
        // general::GeneralMessage's data field should be std::vector<uint8_t> or similar.
        general_msg_instance_.data().assign(string_message.begin(), string_message.end());
        return writer_->write(&general_msg_instance_);
    }

    // --- Member Variables ---
    std::string topic_name_;
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Publisher* dds_publisher_;
    eprosima::fastdds::dds::Topic* topic_;
    eprosima::fastdds::dds::DataWriter* writer_;
    eprosima::fastdds::dds::TypeSupport type_support_; // Manages GeneralMessagePubSubType
    general::GeneralMessage general_msg_instance_; // Re-usable instance for publishing
    DdsPublisherListener listener_; // Listener instance
};

} // namespace link
