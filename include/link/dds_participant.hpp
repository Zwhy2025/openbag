#pragma once

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <memory>
#include <stdexcept>

namespace Link {

class Participant
{
public:
    static eprosima::fastdds::dds::DomainParticipant* GetParticipant()
    {
        static auto participant = createParticipant();
        return participant.get();
    }

    // 禁止构造、拷贝、赋值
    Participant() = delete;
    ~Participant() = delete;
    Participant(const Participant&) = delete;
    Participant& operator=(const Participant&) = delete;

private:
    static std::unique_ptr<eprosima::fastdds::dds::DomainParticipant, void (*)(eprosima::fastdds::dds::DomainParticipant*)> createParticipant()
    {
        auto* raw_participant = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(0, eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT);

        if (!raw_participant)
        {
            throw std::runtime_error("Failed to create DomainParticipant");
        }

        return std::unique_ptr<eprosima::fastdds::dds::DomainParticipant, void (*)(eprosima::fastdds::dds::DomainParticipant*)>(
            raw_participant, [](eprosima::fastdds::dds::DomainParticipant* p) {
                if (p)
                {
                    eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(p);
                }
            });
    }
};

}  // namespace Link
