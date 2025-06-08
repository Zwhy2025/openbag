#pragma once

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace Link {

namespace Participant {

inline eprosima::fastdds::dds::DomainParticipant* s_domain_participant_ = nullptr;

inline eprosima::fastdds::dds::DomainParticipant* get_participant()
{
    if (s_domain_participant_ == nullptr)
    {
        s_domain_participant_ = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(0, eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT);
        if (s_domain_participant_ == nullptr)
        {
            throw std::runtime_error("Failed to create DomainParticipant.");
        }
    }
    return s_domain_participant_;
}

}  // namespace Participant

}  // namespace Link
