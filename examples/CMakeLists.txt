set(ALL_MESSAGE_LIBS ""
    CACHE INTERNAL "所有生成的消息库列表"
)
#-----------------------------------------------------------------------
# 通用消息生成宏: 支持 Fast DDS IDL(.idl) 和 Protobuf(.proto)
# 用法: GENERATE_MESSAGE_CODE(<输入文件> <输出目录>)
# 生成目标库: <消息名>_message
# 全局变量 ALL_MESSAGE_LIBS 保存所有生成的消息库
#-----------------------------------------------------------------------
function(GENERATE_MESSAGE_CODE msg_file output_dir)
    # 确保输出目录存在
    file(MAKE_DIRECTORY ${output_dir})

    # 提取文件名和后缀
    get_filename_component(msg_name ${msg_file} NAME_WE)
    get_filename_component(ext      ${msg_file} EXT)
    get_filename_component(msg_dir  ${msg_file} DIRECTORY)
    get_filename_component(msg_file_name ${msg_file} NAME)

    set(lib_name ${msg_name}_message)

    if(ext STREQUAL ".idl")
        # 查找 fastddsgen
        find_program(FASTDDSGEN_EXECUTABLE NAMES fastddsgen)
        if(NOT FASTDDSGEN_EXECUTABLE)
            message(FATAL_ERROR "fastddsgen 未找到，请先安装 Fast DDS 工具链")
        endif()

        set(gen_hdrs
            ${output_dir}/${msg_name}.h
            ${output_dir}/${msg_name}PubSubTypes.h
        )
        set(gen_srcs
            ${output_dir}/${msg_name}.cxx
            ${output_dir}/${msg_name}PubSubTypes.cxx
        )

        add_custom_command(
            OUTPUT ${gen_hdrs} ${gen_srcs}
            COMMAND ${FASTDDSGEN_EXECUTABLE}
                    -d ${output_dir}
                    ${msg_file}
            DEPENDS ${msg_file}
            COMMENT "[Fast DDS] 从 IDL 生成代码: ${msg_file}"
        )
        add_custom_target(${msg_name}_idl ALL DEPENDS ${gen_hdrs} ${gen_srcs})
    elseif(ext STREQUAL ".proto")
        # 查找 Protobuf
        find_package(Protobuf REQUIRED)

        set(gen_hdrs  ${output_dir}/${msg_name}.pb.h)
        set(gen_srcs  ${output_dir}/${msg_name}.pb.cc)

        add_custom_command(
            OUTPUT ${gen_hdrs} ${gen_srcs}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                    --cpp_out=${output_dir}
                    -I ${msg_dir}
                    ${msg_file_name}
            DEPENDS ${msg_file}
            COMMENT "[Protobuf] 从 .proto 生成代码: ${msg_file}"
        )
        add_custom_target(${msg_name}_proto ALL DEPENDS ${gen_hdrs} ${gen_srcs})
    else()
        message(FATAL_ERROR "不支持的消息后缀: ${ext}")
    endif()

    # 定义静态库来编译生成的源文件
    add_library(${lib_name} STATIC ${gen_srcs})
    if (ext STREQUAL ".idl")
        add_dependencies(${lib_name} ${msg_name}_idl)
    elseif (ext STREQUAL ".proto")
        add_dependencies(${lib_name} ${msg_name}_proto)
    endif()
    target_include_directories(${lib_name} PUBLIC ${output_dir})

    # 将生成库加入全局列表
    list(APPEND ALL_MESSAGE_LIBS ${lib_name})
    set(ALL_MESSAGE_LIBS ${ALL_MESSAGE_LIBS} PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# 使用示例
#-----------------------------------------------------------------------
GENERATE_MESSAGE_CODE(
    ${CMAKE_CURRENT_SOURCE_DIR}/message/general.idl
    ${CMAKE_BINARY_DIR}/generated
)
GENERATE_MESSAGE_CODE(
    ${CMAKE_CURRENT_SOURCE_DIR}/message/test.proto
    ${CMAKE_BINARY_DIR}/generated
)

# 公共库列表
# COMMON_LIBS is now inherited from parent scope and already includes fastcdr and fastrtps

# 生成示例可执行文件并链接所有消息库
foreach(exec IN ITEMS
        op_topic_recorder
        op_topic_player
        op_topic_publisher
        op_topic_subscriber
    )
    add_executable(${exec} ${exec}.cc)
    # Link against openbag_core, common libs, and all message specific libs
    target_link_libraries(${exec} PRIVATE openbag_core ${COMMON_LIBS} ${ALL_MESSAGE_LIBS})
endforeach()

message(STATUS "消息生成与示例配置完成。")


message(STATUS "Examples 配置完成") 