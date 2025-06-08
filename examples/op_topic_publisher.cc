#include <chrono>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "general.h"
#include "generalPubSubTypes.h"

using namespace eprosima::fastdds::dds;
using namespace General;

class MyPublisher
{
public:
    MyPublisher() : participant_(nullptr), publisher_(nullptr), topic_(nullptr), writer_(nullptr), type_(new MessagePubSubType()) {}

    ~MyPublisher()
    {
        if (writer_ != nullptr)
        {
            publisher_->delete_datawriter(writer_);
        }
        if (topic_ != nullptr)
        {
            participant_->delete_topic(topic_);
        }
        if (publisher_ != nullptr)
        {
            participant_->delete_publisher(publisher_);
        }
        if (participant_ != nullptr)
        {
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init()
    {
        DomainParticipantQos pqos;
        pqos.name("PublisherParticipant");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);

        if (participant_ == nullptr)
        {
            return false;
        }

        type_.register_type(participant_);

        PublisherQos pubqos;
        publisher_ = participant_->create_publisher(pubqos, nullptr);

        if (publisher_ == nullptr)
        {
            return false;
        }

        TopicQos tqos;
        tqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        tqos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
        topic_ = participant_->create_topic("GeneralTopic", "General::Message", tqos);

        if (topic_ == nullptr)
        {
            return false;
        }

        DataWriterQos wqos;
        wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        wqos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
        writer_ = publisher_->create_datawriter(topic_, wqos, nullptr);

        if (writer_ == nullptr)
        {
            return false;
        }

        std::cout << "Publisher initialized." << std::endl;
        return true;
    }

    void run(uint32_t samples)
    {
        Message msg;
        msg.header().id("publisher-id");
        msg.header().type("example-message");
        msg.header().version("1.0");

        for (uint32_t i = 0; i < samples; ++i)
        {
            msg.header().timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            msg.payload(std::vector<uint8_t>(i % 10 + 1, (uint8_t)('A' + (i % 26))));

            writer_->write(&msg);
            std::cout << "Publishing message " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

protected:
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;
};

int main(int argc, char** argv)
{
    MyPublisher publisher;
    if (publisher.init())
    {
        publisher.run(10);  // Publish 10 samples
    }
    return 0;
}