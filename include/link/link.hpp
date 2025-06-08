#pragma once

#include <functional>
#include <memory>
#include <string>
#include <mutex> // For thread-safe participant creation

// DDS Includes for DomainParticipant
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

// Forward declare DdsPublisher and DdsSubscriber to avoid full include here if they are heavy,
// but for factory methods returning shared_ptr to them, we need full definition.
// So, include the headers.
#include "link_dds_publisher.hpp" // Defines link::DdsPublisher
#include "link_dds_subscriber.hpp" // Defines link::DdsSubscriber

namespace link {

// Abstract base class for Publishers
template <typename T>
class PublisherBase {
public:
    virtual ~PublisherBase() = default;
    virtual bool publish(const T& message) = 0;
    virtual const std::string& get_topic_name() const = 0;
};

// Abstract base class for Subscribers
template <typename T>
class SubscriberBase {
public:
    virtual ~SubscriberBase() = default;
    virtual const std::string& get_topic_name() const = 0;
};

// Main Link class with static factory methods
class Link {
public:
    template <typename T>
    static std::shared_ptr<PublisherBase<T>> CreatePublisher(const std::string& topic_name) {
        eprosima::fastdds::dds::DomainParticipant* participant = get_dds_participant();
        if (!participant) {
            // std::cerr << "Link ERROR: Failed to get/create DomainParticipant for publisher on topic " << topic_name << std::endl;
            return nullptr; // Or throw
        }
        try {
            return std::make_shared<DdsPublisher<T>>(topic_name, participant);
        } catch (const std::exception& e) {
            // std::cerr << "Link ERROR: Exception during DdsPublisher creation for topic " << topic_name << ": " << e.what() << std::endl;
            return nullptr;
        }
    }

    template <typename T>
    static std::shared_ptr<SubscriberBase<T>> CreateSubscriber(
        const std::string& topic_name,
        std::function<void(const T&)> callback) {
        eprosima::fastdds::dds::DomainParticipant* participant = get_dds_participant();
        if (!participant) {
            // std::cerr << "Link ERROR: Failed to get/create DomainParticipant for subscriber on topic " << topic_name << std::endl;
            return nullptr; // Or throw
        }
        try {
            return std::make_shared<DdsSubscriber<T>>(topic_name, participant, callback);
        } catch (const std::exception& e) {
            // std::cerr << "Link ERROR: Exception during DdsSubscriber creation for topic " << topic_name << ": " << e.what() << std::endl;
            return nullptr;
        }
    }

    // Optional: Method to explicitly release the participant if needed for clean shutdown
    static void ReleaseParticipant() {
        std::lock_guard<std::mutex> lock(participant_mutex_);
        if (dds_participant_ != nullptr) {
            // Important: Ensure all publishers/subscribers are destroyed before deleting participant
            // This basic singleton doesn't manage that; users of Link class must ensure.
            auto factory = eprosima::fastdds::dds::DomainParticipantFactory::get_instance();
            factory->delete_participant(dds_participant_);
            dds_participant_ = nullptr;
            // std::cout << "Link: DDS DomainParticipant released." << std::endl;
        }
    }

private:
    static eprosima::fastdds::dds::DomainParticipant* get_dds_participant() {
        std::lock_guard<std::mutex> lock(participant_mutex_);
        if (dds_participant_ == nullptr) {
            // Default DDS domain ID is 0
            // You can customize DomainParticipantQos here if needed
            dds_participant_ = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(
                0, eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT);

            if (dds_participant_ == nullptr) {
                // std::cerr << "Link FATAL: Failed to create DDS DomainParticipant." << std::endl;
            } else {
                // std::cout << "Link: DDS DomainParticipant created." << std::endl;
                // Optional: Register a shutdown hook to release participant if not done manually
                // std::atexit(ReleaseParticipant); // Be careful with std::atexit and static objects destruction order
            }
        }
        return dds_participant_;
    }

    // Singleton DomainParticipant and its mutex
    // These must be static because get_dds_participant is static
    // Define static members inline (C++17 feature)
    inline static eprosima::fastdds::dds::DomainParticipant* dds_participant_ = nullptr;
    inline static std::mutex participant_mutex_;
    // No separate definition in a .cpp file will be needed for these.
};

} // namespace link
