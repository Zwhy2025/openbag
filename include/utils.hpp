#pragma once

#include <unistd.h>

#include <string>

namespace utils {
/**
 * @brief 获取当前可执行程序路径
 * @return 当前可执行程序路径
 */
inline std::string GetCurrentExecutablePath()
{
    char buffer[1024];
    ssize_t count = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (count != -1)
    {
        buffer[count] = '\0';
        return std::string(buffer);
    }
    return "";
}

/**
 * @brief 设置当前工作路径
 * @param path 工作路径
 */
inline void SetCurrentWorkingDirectory(const std::string& path) { chdir(path.c_str()); }

/**
 * @brief 获取当前工作路径
 * @return 当前工作路径
 */
inline std::string GetCurrentWorkingDirectory()
{
    char buffer[1024];
    getcwd(buffer, sizeof(buffer));
    return std::string(buffer);
}

}  // namespace utils