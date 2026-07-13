# 学习笔记

## V4L2 关键 ioctl

```
VIDIOC_QUERYCAP    — 查询设备能力
VIDIOC_S_FMT       — 设置视频格式（分辨率、像素格式）
VIDIOC_REQBUFS     — 申请缓冲区
VIDIOC_QUERYBUF    — 查询缓冲区信息
VIDIOC_QBUF        — 缓冲区入队
VIDIOC_DQBUF       — 缓冲区出队（取帧）
VIDIOC_STREAMON    — 开始采集
VIDIOC_STREAMOFF   — 停止采集
```

## GStreamer 常用 pipeline

```bash
# 预览
gst-launch-1.0 v4l2src device=/dev/video10 ! videoconvert ! autovideosink

# 软编录 MP4
gst-launch-1.0 -e v4l2src device=/dev/video10 ! videoconvert ! \
  x264enc ! mp4mux ! filesink location=test.mp4

# 硬编（RK3568）
gst-launch-1.0 -e v4l2src ! videoconvert ! mpph264enc ! h264parse ! \
  mp4mux ! filesink location=test.mp4
```

## 排查命令

```bash
ls /dev/video*
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video10 --all
dmesg | tail -50
```

## 分阶段实现

| 阶段 | 内容 |
|------|------|
| 第 1～2 周 | V4L2 采帧、存 YUV |
| 第 3～4 周 | GStreamer 录 MP4、RTSP |
| 第 5～6 周 | 帧率、断流重连、日志 |
| 第 7～8 周 | HTTP API、简单 HTML |
