#pragma once

#include <memory>

#include "link_publisher.hpp"
#include "link_subscriber.hpp"

namespace Link {

template <typename T>
using PublisherPtr = std::shared_ptr<Link::PublisherBase<T>>;

using SubscriberPtr = std::shared_ptr<Link::SubscriberBase>;

}  // namespace Link