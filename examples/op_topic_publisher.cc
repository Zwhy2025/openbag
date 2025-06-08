#include "link/link.hpp" // For the new Link module
#include "examples/message/generated/test.pb.h" // For the Test Protobuf message

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "Starting new publisher example..." << std::endl;

    // Create a string publisher
    auto string_publisher = link::Link::CreatePublisher<std::string>("string_topic_test");
    if (!string_publisher) {
        std::cerr << "Failed to create string publisher!" << std::endl;
        return -1;
    }
    std::cout << "String publisher created for topic: " << string_publisher->get_topic_name() << std::endl;

    // Create a Protobuf Test message publisher
    auto proto_publisher = link::Link::CreatePublisher<Test>("proto_topic_test");
    if (!proto_publisher) {
        std::cerr << "Failed to create proto publisher!" << std::endl;
        return -1;
    }
    std::cout << "Proto publisher created for topic: " << proto_publisher->get_topic_name() << std::endl;

    // Publishing loop
    for (int i = 0; i < 10; ++i) {
        // Publish a string message
        std::string str_message = "Hello from Link String Publisher! Count: " + std::to_string(i);
        if (string_publisher->publish(str_message)) {
            std::cout << "Sent string: "" << str_message << """ << std::endl;
        } else {
            std::cerr << "Failed to send string message." << std::endl;
        }

        // Publish a Protobuf message
        Test test_message;
        test_message.set_id(i);
        test_message.set_message("Hello from Link Proto Publisher! Count: " + std::to_string(i));
        if (proto_publisher->publish(test_message)) {
            std::cout << "Sent proto: id=" << test_message.id() << ", message="" << test_message.message() << """ << std::endl;
        } else {
            std::cerr << "Failed to send proto message." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "Publisher example finished." << std::endl;

    // Allow some time for messages to be sent before participant might be released implicitly by program end.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    link::Link::ReleaseParticipant(); // Optional: explicit cleanup

    return 0;
}
