#!/bin/bash
set -e

# 设置非交互式环境变量，避免交互式配置提示
export DEBIAN_FRONTEND=noninteractive

apt-get install -y --no-install-recommends \
    systemd \
    systemd-sysv \
    dbus \
    libprotobuf-dev \
    libzmq3-dev 