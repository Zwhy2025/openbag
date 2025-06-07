#!/usr/bin/env bash
#
# 项目构建脚本
#
# 该脚本用于构建 cmake 项目
# 使用说明：
#  - ./tools/build.sh [参数]
#
# 参数：
#  - --clean     清理构建目录（build）
#  - --help, -h  显示帮助信息
#
# 作者: Zhao Jun
# 日期: 2024-11-21

# 导入公共函数

script_dir=$(dirname $(readlink -f "$0"))

source "$script_dir/function.bash"

# 设置错误处理
set_error_handling

# --- 主流程 ---
main() {
    log_info "开始构建项目..."
    local project_root="$script_dir/../"
    local clean_build_flag=$(parse_cli_args "$@")
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"  # 编译类型为 Debug
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" # 为 VSCode 生成编译命令文件
    )
    
    # 检查ccache并启用
    check_ccache
    
    log_info "脚本目录: $script_dir"
    log_info "项目根目录: $project_root"
    log_info "清理构建标志: $clean_build_flag"
    log_info "CMake 参数: ${cmake_args[*]}"

    build_project \
        "$project_root" \
        "$clean_build_flag" \
        "${cmake_args[@]}"
}

# 执行主流程
main "$@"
