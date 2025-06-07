/**
 * @copyright Copyright (c) 2025 openbag
 *
 * @author Zhao Jun(zwhy2025@gmail.com)
 * @version 0.1
 * @date 2025-05-22
 *
 * @file proto_utils.hpp
 * @brief Protobuf工具函数
 */

#pragma once

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>

#include <iostream>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

namespace openbag {

/**
 * @brief 用于收集proto导入过程中的错误信息
 */
class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
    void AddError(const std::string& filename, int line, int column, const std::string& message) override
    {
        std::cerr << "Proto解析错误: " << filename << ":" << line << ":" << column << ": " << message << std::endl;
    }
};

/**
 * @brief 包装Importer及其依赖对象的类，确保生命周期正确
 */
class ProtoImporterWrapper
{
public:
    explicit ProtoImporterWrapper(const std::vector<std::string>& search_paths)
        : m_source_tree(std::make_unique<google::protobuf::compiler::DiskSourceTree>()), m_error_collector(std::make_unique<ErrorCollector>())
    {
        // 设置Proto文件搜索路径
        for (const auto& path : search_paths)
        {
            m_source_tree->MapPath("", path);
        }

        m_importer = std::make_unique<google::protobuf::compiler::Importer>(m_source_tree.get(), m_error_collector.get());
    }

    google::protobuf::compiler::Importer* get() { return m_importer.get(); }
    google::protobuf::compiler::Importer* operator->() { return m_importer.get(); }
    google::protobuf::compiler::Importer& operator*() { return *m_importer; }

private:
    std::unique_ptr<google::protobuf::compiler::DiskSourceTree> m_source_tree;
    std::unique_ptr<ErrorCollector> m_error_collector;
    std::unique_ptr<google::protobuf::compiler::Importer> m_importer;
};

/**
 * @brief 创建并序列化FileDescriptorSet
 * @param descriptor 消息描述符
 * @return 序列化后的FileDescriptorSet字符串
 */
inline std::string CreateFileDescriptorSetFromDescriptor(const google::protobuf::Descriptor* descriptor)
{
    google::protobuf::FileDescriptorSet fileDescriptorSet;

    // 添加消息所在的文件描述符
    if (descriptor)
    {
        const google::protobuf::FileDescriptor* fileDesc = descriptor->file();
        google::protobuf::FileDescriptorProto fileProto;
        fileDesc->CopyTo(&fileProto);
        *fileDescriptorSet.add_file() = fileProto;

        // 添加依赖的文件描述符
        for (int i = 0; i < fileDesc->dependency_count(); ++i)
        {
            const google::protobuf::FileDescriptor* depFile = fileDesc->dependency(i);
            google::protobuf::FileDescriptorProto depProto;
            depFile->CopyTo(&depProto);
            *fileDescriptorSet.add_file() = depProto;
        }
    }

    return fileDescriptorSet.SerializeAsString();
}

/**
 * @brief 构建一个包含指定消息描述符及其所有传递依赖项的FileDescriptorSet
 * @param toplevelDescriptor 顶层消息描述符
 * @return FileDescriptorSet对象
 */
inline google::protobuf::FileDescriptorSet BuildFileDescriptorSet(const google::protobuf::Descriptor* toplevelDescriptor)
{
    if (toplevelDescriptor == nullptr)
    {
        return {};
    }
    google::protobuf::FileDescriptorSet fdSet;
    std::queue<const google::protobuf::FileDescriptor*> pending;
    std::unordered_set<std::string> seen;

    pending.push(toplevelDescriptor->file());
    seen.insert(toplevelDescriptor->file()->name());

    while (!pending.empty())
    {
        const google::protobuf::FileDescriptor* fdesc = pending.front();
        pending.pop();

        // 复制文件描述符到集合
        fdesc->CopyTo(fdSet.add_file());

        // 遍历依赖项
        for (int i = 0; i < fdesc->dependency_count(); ++i)
        {
            const google::protobuf::FileDescriptor* dep = fdesc->dependency(i);
            const std::string& name = dep->name();
            if (seen.insert(name).second)
            {
                pending.push(dep);
            }
        }
    }

    return fdSet;
}

/**
 * @brief 创建Proto导入器
 * @param search_paths Proto文件搜索路径
 * @return Proto导入器包装器
 */
inline std::unique_ptr<ProtoImporterWrapper> CreateProtoImporter(const std::vector<std::string>& search_paths) { return std::make_unique<ProtoImporterWrapper>(search_paths); }

}  // namespace openbag