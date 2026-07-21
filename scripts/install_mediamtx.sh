#!/bin/bash
# 下载 MediaMTX（ARM64 / x86_64）

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MEDIAMTX_BIN="${SCRIPT_DIR}/mediamtx"
VERSION="v1.9.3"
ARCH="$(uname -m)"

case "${ARCH}" in
    aarch64|arm64)
        PLATFORM="linux_arm64v8"
        ;;
    armv7l|armhf)
        PLATFORM="linux_arm32v7"
        ;;
    x86_64|amd64)
        PLATFORM="linux_amd64"
        ;;
    *)
        echo "不支持的架构: ${ARCH}"
        exit 1
        ;;
esac

if [ -x "${MEDIAMTX_BIN}" ]; then
    echo "MediaMTX 已存在: ${MEDIAMTX_BIN}"
    exit 0
fi

URL="https://github.com/bluenviron/mediamtx/releases/download/${VERSION}/mediamtx_${VERSION}_${PLATFORM}.tar.gz"
TMP="$(mktemp -d)"

echo "==> 下载 MediaMTX ${VERSION} (${PLATFORM}) ..."
if command -v wget >/dev/null 2>&1; then
    wget -O "${TMP}/mediamtx.tar.gz" "${URL}"
elif command -v curl >/dev/null 2>&1; then
    curl -L -o "${TMP}/mediamtx.tar.gz" "${URL}"
else
    echo "请先安装 wget 或 curl"
    exit 1
fi

tar xzf "${TMP}/mediamtx.tar.gz" -C "${TMP}"
install -m 755 "${TMP}/mediamtx" "${MEDIAMTX_BIN}"
rm -rf "${TMP}"

echo "==> 完成: ${MEDIAMTX_BIN}"
