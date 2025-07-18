#!/usr/bin/env bash

# 获取当前脚本所在目录
script_dir=$(dirname $(readlink -f "$0"))
ws_dir=$(dirname $script_dir)

source "$script_dir/function.bash"

set_error_handling

# --- 主流程 ---
main() {
  
  # 命令检查
  ensure_commands \
    sudo \
    pip3 \
    cmake \
    curl \
    zip  
    
  # 检查 root 权限警告
  check_root_warning

  # 检查Python版本
  check_python_warning

  install_apt_packages \
    libyaml-cpp-dev \
    liblz4-dev \
    libzstd-dev \
    protobuf-compiler \
    libprotobuf-dev \
    libprotoc-dev \
    libfastcdr-dev \
    libfastrtps-dev \
    fastddsgen 

  # 检查 mcap 库
  build_from_source \
      "mcap" \
      "https://github.com/Zwhy2025/mcap_builder.git" \
      "main" \
      "true" \
      "-DCMAKE_INSTALL_PREFIX=/usr" \
      "make_install" 

  # Download mcap binary with error handling
  if ! wget https://github.com/foxglove/mcap/releases/download/releases%2Fmcap-cli%2Fv0.0.50/mcap-linux-amd64 -O "$ws_dir/mcap-linux-amd64"; then
    log_error "Failed to download mcap binary"
    exit 1
  fi
  
  # Verify the binary was downloaded successfully
  if [[ ! -f "$ws_dir/mcap-linux-amd64" ]]; then
    log_error "mcap binary not found after download"
    exit 1
  fi
  
  chmod +x "$ws_dir/mcap-linux-amd64"
    
  log_success "所有依赖安装成功。"
}

main
