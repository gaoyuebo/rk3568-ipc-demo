#!/bin/bash
# RTSP 推流，PC 端 VLC 打开 rtsp://<板子IP>:8554/live
#
# 用法:
#   ./scripts/rtsp_push.sh [device] [port]
#   ./scripts/rtsp_push.sh /dev/video10 8554

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE="${1:-/dev/video10}"
PORT="${2:-8554}"
PATH_NAME="${RTSP_PATH:-live}"
WIDTH="${RTSP_WIDTH:-640}"
HEIGHT="${RTSP_HEIGHT:-480}"
FRAMERATE="${RTSP_FPS:-15}"

MEDIAMTX_PID=""
PUBLISHER_PID=""

cleanup() {
    if [ -n "${PUBLISHER_PID}" ]; then
        kill "${PUBLISHER_PID}" 2>/dev/null || true
        wait "${PUBLISHER_PID}" 2>/dev/null || true
    fi
    if [ -n "${MEDIAMTX_PID}" ]; then
        kill "${MEDIAMTX_PID}" 2>/dev/null || true
        wait "${MEDIAMTX_PID}" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

find_test_launch() {
    if command -v test-launch >/dev/null 2>&1; then
        command -v test-launch
        return 0
    fi
    if [ -x "${SCRIPT_DIR}/test-launch" ]; then
        echo "${SCRIPT_DIR}/test-launch"
        return 0
    fi
    return 1
}

ensure_mediamtx() {
    if command -v mediamtx >/dev/null 2>&1; then
        echo "$(command -v mediamtx)"
        return 0
    fi
    if [ -x "${SCRIPT_DIR}/mediamtx" ]; then
        echo "${SCRIPT_DIR}/mediamtx"
        return 0
    fi
    if [ -f "${SCRIPT_DIR}/install_mediamtx.sh" ]; then
        bash "${SCRIPT_DIR}/install_mediamtx.sh"
    fi
    if [ -x "${SCRIPT_DIR}/mediamtx" ]; then
        echo "${SCRIPT_DIR}/mediamtx"
        return 0
    fi
    return 1
}

run_test_launch() {
    local launcher="$1"
    local pipeline

    pipeline="( v4l2src device=${DEVICE} ! videoconvert ! x264enc tune=zerolatency speed-preset=ultrafast ! rtph264pay name=pay0 pt=96 )"

    echo "==> 使用 test-launch 启动 RTSP ..."
    echo "VLC URL: rtsp://<board-ip>:${PORT}/test"
    echo "Ctrl+C to stop"
    trap - EXIT INT TERM
    exec "${launcher}" -p "${PORT}" "${pipeline}"
}

start_mediamtx() {
    local bin="$1"
    local config="${SCRIPT_DIR}/mediamtx.yml"

    if command -v ss >/dev/null 2>&1 && ss -ltn 2>/dev/null | grep -q ":${PORT} "; then
        echo "==> RTSP 端口 ${PORT} 已在监听，复用已有 MediaMTX 服务"
        MEDIAMTX_PID=""
        return 0
    fi

    pkill -f "${SCRIPT_DIR}/mediamtx" 2>/dev/null || true
    sleep 1

    echo "==> 启动 MediaMTX (port ${PORT}, TCP only) ..."
    if [ -f "${config}" ] && [ "${PORT}" = "8554" ]; then
        "${bin}" "${config}" >/tmp/mediamtx.log 2>&1 &
    else
        MTX_RTSPADDRESS=":${PORT}" \
        MTX_PROTOCOLS="tcp" \
        MTX_RTMP="no" \
        MTX_HLS="no" \
        MTX_WEBRTC="no" \
        MTX_SRT="no" \
        MTX_PATHS_LIVE_SOURCE="publisher" \
        "${bin}" >/tmp/mediamtx.log 2>&1 &
    fi
    MEDIAMTX_PID=$!
    sleep 1

    if ! kill -0 "${MEDIAMTX_PID}" 2>/dev/null; then
        echo "MediaMTX 启动失败，日志:"
        cat /tmp/mediamtx.log 2>/dev/null || true
        echo ""
        echo "排查建议:"
        echo "  sudo ss -ltnp | grep ${PORT}"
        echo "  sudo ss -ulnp | grep -E '8000|8001'"
        echo "  sudo killall mediamtx"
        return 1
    fi
    return 0
}

run_gst_publisher() {
    local rtsp_url="rtsp://127.0.0.1:${PORT}/${PATH_NAME}"

    echo "==> 使用 GStreamer 推流到 ${rtsp_url} ..."
    gst-launch-1.0 -e \
        v4l2src device="${DEVICE}" ! \
        "video/x-raw,width=${WIDTH},height=${HEIGHT}" ! \
        videoconvert ! \
        x264enc tune=zerolatency speed-preset=ultrafast bitrate=600 key-int-max=30 ! \
        "video/x-h264,profile=baseline" ! \
        rtspclientsink location="${rtsp_url}" protocols=tcp &
    PUBLISHER_PID=$!
    wait "${PUBLISHER_PID}"
}

run_ffmpeg_publisher() {
    local rtsp_url="rtsp://127.0.0.1:${PORT}/${PATH_NAME}"

    echo "==> 使用 ffmpeg 推流到 ${rtsp_url} ..."
    ffmpeg -loglevel warning -y \
        -f v4l2 -input_format yuyv422 -video_size "${WIDTH}x${HEIGHT}" -framerate "${FRAMERATE}" \
        -i "${DEVICE}" \
        -c:v libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -g "${FRAMERATE}" -b:v 600k \
        -f rtsp -rtsp_transport tcp "${rtsp_url}" &
    PUBLISHER_PID=$!
    wait "${PUBLISHER_PID}"
}

run_mediamtx_mode() {
    local mediamtx_bin

    mediamtx_bin="$(ensure_mediamtx)" || {
        echo "MediaMTX 不可用，请执行: ./scripts/install_mediamtx.sh"
        exit 1
    }

    start_mediamtx "${mediamtx_bin}" || exit 1

    echo "VLC URL: rtsp://<board-ip>:${PORT}/${PATH_NAME}"
    echo "Ctrl+C to stop"

    if command -v ffmpeg >/dev/null 2>&1; then
        if run_ffmpeg_publisher; then
            return 0
        fi
        echo "ffmpeg 推流失败，尝试 GStreamer ..."
    fi

    if gst-inspect-1.0 rtspclientsink >/dev/null 2>&1; then
        if run_gst_publisher; then
            return 0
        fi
    fi

    echo "缺少推流工具，请安装:"
    echo "  sudo apt install -y ffmpeg"
    echo "  sudo apt install -y gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly"
    exit 1
}

echo "RTSP push: device=${DEVICE}, port=${PORT}, path=/${PATH_NAME}"

if launcher="$(find_test_launch)"; then
    if [ "${RTSP_FORCE_MEDIAMTX}" != "1" ]; then
        run_test_launch "${launcher}"
    fi
fi

echo "未找到 test-launch，使用 MediaMTX 方案 ..."
run_mediamtx_mode
