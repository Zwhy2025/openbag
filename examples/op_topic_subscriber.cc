#include <chrono>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "general.h"
#include "generalPubSubTypes.h"

using namespace eprosima::fastdds::dds;
using namespace General;

class MySubscriber
{
public:
    MySubscriber() : participant_(nullptr), subscriber_(nullptr), topic_(nullptr), reader_(nullptr), type_(new MessagePubSubType()) {}

    ~MySubscriber()
    {
        if (reader_ != nullptr)
        {
            subscriber_->delete_datareader(reader_);
        }
        if (topic_ != nullptr)
        {
            participant_->delete_topic(topic_);
        }
        if (subscriber_ != nullptr)
        {
            participant_->delete_subscriber(subscriber_);
        }
        if (participant_ != nullptr)
        {
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }
    }

    bool init()
    {
        DomainParticipantQos pqos;
        pqos.name("SubscriberParticipant");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);

        if (participant_ == nullptr)
        {
            return false;
        }

        type_.register_type(participant_);

        SubscriberQos subqos;
        subscriber_ = participant_->create_subscriber(subqos, nullptr);

        if (subscriber_ == nullptr)
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

        DataReaderQos rqos;
        rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        rqos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
        reader_ = subscriber_->create_datareader(topic_, rqos, &listener_);

        if (reader_ == nullptr)
        {
            return false;
        }

        std::cout << "Subscriber initialized." << std::endl;
        return true;
    }

    void run()
    {
        std::cout << "Waiting for data..." << std::endl;
        // The subscriber will run indefinitely, handled by the listener
        // You might want to add a mechanism to stop it, e.g., a signal handler
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

protected:
    DomainParticipant* participant_;
    Subscriber* subscriber_;
    Topic* topic_;
    DataReader* reader_;
    TypeSupport type_;

    class SubListener : public DataReaderListener
    {
    public:
        SubListener() : samples_(0) {}

        ~SubListener() {}

        void on_data_available(DataReader* reader) override
        {
            Message msg;
            SampleInfo info;

            if (reader->take_next_sample(&msg, &info) == ReturnCode_t::RETCODE_OK)
            {
                if (info.instance_state == ALIVE_INSTANCE_STATE)
                {
                    samples_++;
                    std::cout << "Message " << msg.header().id() << " with payload size " << msg.payload().size() << " received: " << msg.header().timestamp() << std::endl;
                }
            }
        }

        void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) override
        {
            if (info.current_count_change == 1)
            {
                std::cout << "Subscriber matched a publisher." << std::endl;
            } else if (info.current_count_change == -1)
            {
                std::cout << "Subscriber unmatched a publisher." << std::endl;
            }
        }

        int samples_;
    } listener_;
};

int main(int argc, char** argv)
{
    MySubscriber subscriber;
    if (subscriber.init())
    {
        subscriber.run();
    }
    return 0;
}