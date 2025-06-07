cmake_minimum_required(VERSION 3.22.1)
project(openbag LANGUAGES CXX)

#-----------------------------------------------------------------------
# 基础配置
#-----------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-----------------------------------------------------------------------
# 模块路径与依赖查找
#-----------------------------------------------------------------------
find_package(yaml-cpp REQUIRED)
find_package(Protobuf REQUIRED)
find_package(mcap REQUIRED)

#-----------------------------------------------------------------------
# 包含目录与链接库配置
#-----------------------------------------------------------------------
set(PROJECT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Protobuf_INCLUDE_DIRS}
    ${MCAP_INCLUDE_DIRS}
)

include_directories(${PROJECT_INCLUDES})

# 链接库集合
set(COMMON_LIBS
    yaml-cpp
    pthread
    mcap
    ${Protobuf_LIBRARIES}
)

#-----------------------------------------------------------------------
# 示例程序
#-----------------------------------------------------------------------
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

