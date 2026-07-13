#!/bin/bash
# 安装 RK3568 IPC demo 依赖

set -e

echo "==> 更新 apt 索引..."
sudo apt update

echo "==> 安装 V4L2 / GStreamer 依赖..."
sudo apt install -y \
    build-essential \
    v4l-utils \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    libgstreamer1.0-dev

echo "==> 检查摄像头设备..."
ls -l /dev/video* 2>/dev/null || echo "未找到 /dev/video*，请插入摄像头后重试"

echo "==> 完成"
