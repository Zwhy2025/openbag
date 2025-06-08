#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "link/link_publisher.hpp"
#include "signal.h"
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
    if (!string_publisher)
    {
        std::cerr << "Failed to create string publisher!" << std::endl;
        return -1;
    }
    std::cout << "String publisher created for topic: " << string_publisher->GetTopicName() << std::endl;

    auto proto_publisher = Link::CreatePublisher<test::TestMessage>("proto_topic_test");
    if (!proto_publisher)
    {
        std::cerr << "Failed to create proto publisher!" << std::endl;
        return -1;
    }
    std::cout << "Proto publisher created for topic: " << proto_publisher->GetTopicName() << std::endl;

    int i = 0;
    while (keep_running)
    {
        std::string str_message = "Hello from Link String Publisher! Count: " + std::to_string(i);
        if (string_publisher->Publish(str_message))
        {
            std::cout << "Sent string: " << str_message << std::endl;
        } else
        {
            std::cerr << "Failed to send string message." << std::endl;
        }

        test::TestMessage test_message;
        test_message.set_id(i);
        test_message.set_name("Hello from Link Proto Publisher! Count: " + std::to_string(i));
        if (proto_publisher->Publish(test_message))
        {
            std::cout << "Sent proto: id=" << test_message.id() << ", message=" << test_message.DebugString() << std::endl;
        } else
        {
            std::cerr << "Failed to send proto message." << std::endl;
        }

        i++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Publisher example finished." << std::endl;
    return 0;
}
