#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "general.h"
#include "link/link_transport.hpp"
#include "openbag/config.hpp"
#include "openbag/recorder.hpp"
#include "openbag/transport.hpp"
#include "test.pb.h"
#include "utils.hpp"
extern openbag::MessageAdapterFactoryPtr GetLinkAdapterFactory();

int main(int argc, char* argv[])
{
    // 设置当前工作路径为可执行程序路径
    std::string executablePath = utils::GetCurrentExecutablePath();
    if (!executablePath.empty())
    {
        std::string cwd = executablePath + "/../../../";
        utils::SetCurrentWorkingDirectory(cwd);
    }

    openbag::ConfigManager configManager;

    // 加载录制配置
    configManager.LoadRecorderConfig("config/recorder.yaml");
    configManager.LoadBufferConfig("config/buffer.yaml");
    configManager.LoadStorageConfig("config/storage.yaml");

    auto storageConfig = configManager.GetStorageConfig();
    storageConfig.proto_search_paths.push_back("/workspace/examples/message");
    configManager.SetStorageConfig(storageConfig);

    // 创建消息适配器工厂
    auto adapterFactory = GetLinkAdapterFactory();

    // 创建录制器
    openbag::Recorder recorder(configManager, adapterFactory);

    std::cout << "正在启动录制器..." << std::endl;
    if (recorder.Start())
    {
        std::cout << "录制器已启动。按 Enter 停止录制。" << std::endl;
        std::cin.get();  // 等待用户按Enter

        std::cout << "正在停止录制器..." << std::endl;
        recorder.Stop();
        std::cout << "录制器已停止。" << std::endl;
    } else
    {
        std::cerr << "启动录制器失败！" << std::endl;
    }

    return 0;
}