#!/bin/bash
# 编译 gst-rtsp-server 的 test-launch 工具

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${SCRIPT_DIR}/test-launch"

if ! pkg-config --exists gstreamer-1.0 gstreamer-rtsp-server-1.0; then
    echo "缺少开发包，请先执行:"
    echo "  sudo apt install -y libgstrtspserver-1.0-dev libglib2.0-dev"
    exit 1
fi

echo "==> 编译 test-launch ..."
gcc "${SCRIPT_DIR}/test-launch.c" -o "${OUT}" \
    $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtsp-server-1.0)

echo "==> 完成: ${OUT}"
