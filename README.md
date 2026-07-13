# rk3568-ipc-demo

基于 RK3568 + 罗技 C270（UVC）的小型 IPC 摄像头固件 demo。

## 功能

- [ ] V4L2 mmap 采集（YUYV）
- [ ] GStreamer H264 编码（x264 软编 / MPP 硬编）
- [ ] MP4 本地录像
- [ ] RTSP 推流（VLC 预览）
- [ ] 帧率统计、断流检测与重连
- [ ] HTTP API 设备控制（可选）

## 硬件

| 项目 | 说明 |
|------|------|
| 开发板 | RK3568 4G+32G |
| 摄像头 | 罗技 C270（UVC 免驱） |
| 系统 | Ubuntu 20.04 / Debian 11 |

## 目录结构

```
rk3568-ipc-demo/
├── README.md
├── docs/
│   ├── architecture.md
│   └── notes.md
├── src/
│   ├── v4l2_capture.c
│   ├── main.c
│   ├── monitor.c
│   └── http_server.c
├── scripts/
│   ├── install_deps.sh
│   ├── gst_record.sh
│   └── rtsp_push.sh
├── Makefile
└── .gitignore
```

## 快速开始

### 1. 安装依赖（板子上）

```bash
chmod +x scripts/*.sh
./scripts/install_deps.sh
```

### 2. 编译

```bash
# 板子上 native 编译
make

# Ubuntu 18.04 虚拟机交叉编译
make cross
# 或
make CC=aarch64-linux-gnu-gcc
```

### 3. 运行 V4L2 采帧

```bash
./capture -d /dev/video10 -w 640 -h 480 -n 100 -o test.yuv
# 默认已是 /dev/video10，可省略 -d
./capture -w 640 -h 480 -n 100 -o test.yuv
```

### 4. GStreamer 录 MP4

```bash
./scripts/gst_record.sh /dev/video10 test.mp4
# 或省略设备参数（默认 /dev/video10）
./scripts/gst_record.sh
```

### 5. RTSP 推流

```bash
./scripts/rtsp_push.sh /dev/video10
# 或
./scripts/rtsp_push.sh
# PC 端 VLC 打开: rtsp://<板子IP>:8554/live
```

## 架构

预览走 RTSP，管理走 HTTP，与商用 IPC 架构一致。详见 [docs/architecture.md](docs/architecture.md)。

## 工作流

```
Windows 写代码 → git commit → git push → GitHub
板子 SSH 登录 → git pull → make → 运行测试
```
