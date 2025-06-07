#!/bin/bash
set -e  # 出错即退出
set -o pipefail

# 设置用户主目录下的安装路径
INSTALL_DIR="$HOME/miniconda3"

# 如果已安装则跳过
if [ -d "$INSTALL_DIR" ]; then
    echo "Miniconda already installed at $INSTALL_DIR"
else
    # 下载并安装 Miniconda
    echo "Installing Miniconda3..."
    wget https://mirrors.tuna.tsinghua.edu.cn/anaconda/miniconda/Miniconda3-latest-Linux-x86_64.sh -O /tmp/miniconda.sh

    bash /tmp/miniconda.sh -b -p "$INSTALL_DIR"
    rm /tmp/miniconda.sh
    
    # 配置 PATH（仅在当前会话生效）
    export PATH="$INSTALL_DIR/bin:$PATH"
    
    # 2.6 **移除所有已有通道（避免默认通道）**
    conda config --remove-key channels || true

    conda init bash

    # 2.7 添加清华镜像通道（并保证顺序：必先添加最主要的 main，再追加其他）
    conda config --add channels https://mirrors.tuna.tsinghua.edu.cn/anaconda/pkgs/main/
    conda config --add channels https://mirrors.tuna.tsinghua.edu.cn/anaconda/pkgs/free/
    conda config --add channels https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/pytorch/
    conda config --add channels https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge/
    
    conda config --set show_channel_urls yes
    
    # 创建 Python 环境
    conda create -n py3.10 python=3.10 -y
    
    # 写入环境变量设置到 bashrc
    echo "export PATH=\"$INSTALL_DIR/bin:\$PATH\"" >> ~/.bashrc
fi

# 激活环境（仅当前会话）
source "$INSTALL_DIR/etc/profile.d/conda.sh" 2>/dev/null
conda activate py3.10

# 验证安装
echo -e "\nInstallation complete. Miniconda path: $INSTALL_DIR"
conda env list
python --version