#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "link/link_transport.hpp"
#include "openbag/config.hpp"
#include "openbag/player.hpp"
#include "openbag/transport.hpp"

extern openbag::MessageAdapterFactoryPtr GetLinkAdapterFactory();

int main(int argc, char* argv[])
{
    openbag::ConfigManager configManager;
    openbag::PlayerConfig playerConfig = configManager.GetPlayerConfig();
    playerConfig.input_path = "openbag_test.mcap";
    configManager.SetPlayerConfig(playerConfig);

    auto adapterFactory = GetLinkAdapterFactory();
    openbag::Player player(playerConfig, adapterFactory);

    std::cout << "Starting player..." << std::endl;
    if (player.Start())
    {
        std::cout << "Player started. Press Enter to stop playback." << std::endl;
        std::cin.get();

        std::cout << "Stopping player..." << std::endl;
        player.Stop();
        std::cout << "Player stopped." << std::endl;
    } else
    {
        std::cerr << "Failed to start player! Please ensure 'openbag_test.mcap' file exists." << std::endl;
    }

    return 0;
}