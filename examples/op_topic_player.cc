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
    configManager.LoadPlayerConfig("config/player.yaml");

    // 获取播放配置
    openbag::PlayerConfig playerConfig = configManager.GetPlayerConfig();
    playerConfig.input_path = "openbag_test.mcap";  // 设置输入文件路径，替换为你的录制文件
    configManager.SetPlayerConfig(playerConfig);

    // 创建消息适配器工厂
    auto adapterFactory = GetLinkAdapterFactory();

    // 创建播放器
    openbag::Player player(playerConfig, adapterFactory);

    std::cout << "正在启动播放器..." << std::endl;
    if (player.Start())
    {
        std::cout << "播放器已启动。按 Enter 停止播放。" << std::endl;
        std::cin.get();  // 等待用户按Enter

        std::cout << "正在停止播放器..." << std::endl;
        player.Stop();
        std::cout << "播放器已停止。" << std::endl;
    } else
    {
        std::cerr << "启动播放器失败！请确保 'my_recording.mcap' 文件存在。" << std::endl;
    }

    return 0;
}