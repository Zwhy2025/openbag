#include <signal.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "link/link_publisher.hpp"
#include "test.pb.h"

std::atomic<bool> keep_running(true);

void signal_handler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
}

int main(int argc, char* argv[])
{
    std::cout << "Starting new publisher example..." << std::endl;
    signal(SIGINT, signal_handler);
    auto string_publisher = Link::CreatePublisher<std::string>("string_topic_test");
    auto proto_publisher = Link::CreatePublisher<test::TestMessage>("proto_topic_test");

    int i = 0;
    while (keep_running)
    {
        std::string str_message = "Hello from Link Publisher! Count: " + std::to_string(i);
        test::TestMessage test_message;
        test_message.set_id(i);
        test_message.set_name(str_message);

        std::vector<double> data;
        for (int j = 0; j < 10; j++)
        {
            test_message.mutable_values()->Add(std::sin(j + i));
        }
        std::string test_message_str = test_message.SerializeAsString();
        if (string_publisher->Publish(test_message_str))
        {
            std::cout << "Published string message: " << test_message.DebugString() << std::endl;
        } else
        {
            std::cerr << "Failed to publish string message." << std::endl;
        }

        if (proto_publisher->Publish(test_message))
        {
            std::cout << "Published proto message: id=" << test_message.id() << ", message=" << test_message.DebugString() << std::endl;
        } else
        {
            std::cerr << "Failed to publish proto message." << std::endl;
        }

        i++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Publisher example finished." << std::endl;
    return 0;
}
