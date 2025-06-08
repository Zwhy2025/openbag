#include "link/link.hpp" // For the new Link module
#include "examples/message/generated/test.pb.h" // For the Test Protobuf message

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic> // For a simple way to keep the subscriber running

// Define a global atomic flag to control the subscriber loop
std::atomic<bool> keep_running(true);

void string_message_callback(const std::string& message) {
    std::cout << "Received string: "" << message << """ << std::endl;
}

void proto_message_callback(const Test& message) {
    std::cout << "Received proto: id=" << message.id() << ", message="" << message.message() << """ << std::endl;
}

// Optional: A simple signal handler for Ctrl+C if you want to run it interactively for longer
// #include <signal.h>
// void signal_handler(int signum) {
//     std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
//     keep_running = false;
// }

int main(int argc, char* argv[]) {
    std::cout << "Starting new subscriber example..." << std::endl;
    // signal(SIGINT, signal_handler); // Optional: Handle Ctrl+C

    // Create a string subscriber
    auto string_subscriber = link::Link::CreateSubscriber<std::string>("string_topic_test", string_message_callback);
    if (!string_subscriber) {
        std::cerr << "Failed to create string subscriber!" << std::endl;
        return -1;
    }
    std::cout << "String subscriber created for topic: " << string_subscriber->get_topic_name() << std::endl;

    // Create a Protobuf Test message subscriber
    auto proto_subscriber = link::Link::CreateSubscriber<Test>("proto_topic_test", proto_message_callback);
    if (!proto_subscriber) {
        std::cerr << "Failed to create proto subscriber!" << std::endl;
        return -1;
    }
    std::cout << "Proto subscriber created for topic: " << proto_subscriber->get_topic_name() << std::endl;

    std::cout << "Subscribers are running. Waiting for messages..." << std::endl;
    std::cout << "(This example will run for 15 seconds, or press Ctrl+C if signal handler is enabled)" << std::endl;

    // Keep the main thread alive to allow subscribers to receive messages
    // For a simple test, sleep for a duration. In a real app, you'd have a proper event loop or condition.
    int run_duration_seconds = 15;
    for(int i = 0; i < run_duration_seconds && keep_running; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Subscriber example finishing." << std::endl;

    // Subscribers will be destroyed when they go out of scope.
    // Explicitly release participant if desired, ensuring subscribers are no longer active.
    // To ensure callbacks are finished, it might be better to release participant before subscribers go out of scope,
    // or ensure main thread outlives subscriber objects' destruction if they rely on the participant.
    // For this example, letting shared_ptrs manage lifetime is fine, then release participant.
    link::Link::ReleaseParticipant(); // Optional: explicit cleanup

    return 0;
}
