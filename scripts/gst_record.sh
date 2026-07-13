#!/bin/bash
# GStreamer 录 MP4（软编 x264enc）

DEVICE="${1:-/dev/video10}"
OUTPUT="${2:-test.mp4}"

echo "Recording from ${DEVICE} to ${OUTPUT} ..."

gst-launch-1.0 -e \
    v4l2src device="${DEVICE}" ! \
    videoconvert ! \
    x264enc tune=zerolatency speed-preset=ultrafast ! \
    mp4mux ! \
    filesink location="${OUTPUT}"

echo "Done: ${OUTPUT}"
