#!/bin/bash
# RTSP 推流，PC 端 VLC 打开 rtsp://<板子IP>:8554/test

DEVICE="${1:-/dev/video10}"
PORT="${2:-8554}"

echo "Starting RTSP server on port ${PORT} ..."
echo "VLC URL: rtsp://<board-ip>:${PORT}/test"

if command -v test-launch >/dev/null 2>&1; then
    test-launch "( v4l2src device=${DEVICE} ! videoconvert ! x264enc tune=zerolatency speed-preset=ultrafast ! rtph264pay name=pay0 pt=96 )"
else
    echo "请先安装 RTSP 组件:"
    echo "  sudo apt install gstreamer1.0-rtsp"
    echo ""
    echo "临时替代方案（UDP 推流到 PC，需 PC 端接收）:"
    echo "  gst-launch-1.0 v4l2src device=${DEVICE} ! videoconvert ! x264enc ! rtph264pay ! udpsink host=<pc-ip> port=5000"
    exit 1
fi
