/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file openbag.hpp
 * @brief openbag主头文件，包含所有核心功能
 */
#pragma once

#include <string>

// 核心组件
#include "buffer.hpp"
#include "common.hpp"
#include "config.hpp"
#include "player.hpp"
#include "proto_utils.hpp"
#include "reader.hpp"
#include "recorder.hpp"
#include "storage.hpp"
#include "transport.hpp"

/**
 * @brief openbag命名空间
 *
 * openbag是一个用于记录和回放Protobuf消息的C++库，支持：
 * - MCAP格式文件的读写
 * - 动态Protobuf消息解析
 * - 消息缓冲和批量处理
 * - 多话题消息录制和回放
 * - 灵活的传输适配器架构
 *
 * 主要特性：
 * - 纯header-only实现，易于集成
 * - 支持MCAP格式的高效存储
 * - 动态Protobuf解析，无需预编译消息类型
 * - 多线程安全的消息处理
 * - 丰富的配置选项和工具函数
 */
namespace openbag {

/**
 * @brief 库版本信息
 */
struct Version
{
    static constexpr int MAJOR = 0;  ///< 主版本号
    static constexpr int MINOR = 1;  ///< 次版本号
    static constexpr int PATCH = 0;  ///< 补丁版本号

    /**
     * @brief 获取版本字符串
     * @return 版本字符串，格式为"major.minor.patch"
     */
    static std::string GetVersionString() { return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(PATCH); }
};

}  // namespace openbag
