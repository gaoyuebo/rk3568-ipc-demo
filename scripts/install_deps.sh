#!/bin/bash
# 安装 RK3568 IPC demo 依赖

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "==> 更新 apt 索引..."
sudo apt update

echo "==> 安装 V4L2 / GStreamer 依赖..."
sudo apt install -y \
    build-essential \
    v4l-utils \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp \
    ffmpeg

echo "==> 安装 MediaMTX (RTSP 服务)..."
bash "${SCRIPT_DIR}/install_mediamtx.sh"

echo "==> 尝试编译 test-launch (可选，KICKPI 镜像可能失败)..."
if bash "${SCRIPT_DIR}/build_test_launch.sh"; then
    echo "test-launch 编译成功"
else
    echo "跳过 test-launch，RTSP 将使用 MediaMTX 方案"
fi

echo "==> 检查摄像头设备..."
ls -l /dev/video* 2>/dev/null || echo "未找到 /dev/video*，请插入摄像头后重试"

echo "==> 完成"
