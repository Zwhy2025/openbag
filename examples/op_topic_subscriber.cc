#include <signal.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "link/link_subscriber.hpp"
#include "test.pb.h"

std::atomic<bool> keep_running(true);

void string_message_callback(const std::string& message)
{
    std::cout << "Received string: " << message << std::endl;
    test::TestMessage test_message;
    test_message.ParseFromString(message);
    std::cout << "Received ParseFromString: " << test_message.DebugString() << std::endl;
}

void proto_message_callback(const test::TestMessage& message)
{
    //
    std::cout << "Received DebugString: " << message.DebugString() << std::endl;
}

void signal_handler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signal_handler);
    std::cout << "Starting new subscriber example..." << std::endl;

    auto string_subscriber = Link::CreateSubscriber<std::string>("string_topic_test", string_message_callback);
    if (!string_subscriber)
    {
        std::cerr << "Failed to create string subscriber!" << std::endl;
        return -1;
    }
    auto proto_subscriber = Link::CreateSubscriber<test::TestMessage>("proto_topic_test", proto_message_callback);
    if (!proto_subscriber)
    {
        std::cerr << "Failed to create proto subscriber!" << std::endl;
        return -1;
    }

    while (keep_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Subscriber example finishing." << std::endl;
    return 0;
}
