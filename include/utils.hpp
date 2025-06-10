#pragma once

#ifdef __unix__
#include <limits.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>

namespace utils {
/**
 * @brief 获取当前可执行程序路径
 * @return 当前可执行程序路径
 */
inline std::string GetCurrentExecutablePath()
{
    std::vector<char> buffer(PATH_MAX);  // Start with a reasonable size, PATH_MAX is usually 4096
    ssize_t count = 0;
    while ((count = readlink("/proc/self/exe", buffer.data(), buffer.size())) == static_cast<ssize_t>(buffer.size()))
    {
        buffer.resize(buffer.size() * 2);  // Double the buffer size if it's too small
    }

    if (count != -1)
    {
        return std::string(buffer.data(), count);
    }
    return "";
}

/**
 * @brief 设置当前工作路径
 * @param path 工作路径
 */
inline bool SetCurrentWorkingDirectory(const std::string& path) { return chdir(path.c_str()) == 0; }

/**
 * @brief 获取当前工作路径
 * @return 当前工作路径
 */
inline std::string GetCurrentWorkingDirectory()
{
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr)
    {
        return std::string(buffer);
    }
    return "";
}

}  // namespace utils