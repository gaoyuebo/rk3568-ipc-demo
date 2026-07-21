# rk3568-ipc-demo

基于 RK3568 + 罗技 C270（UVC）的小型 IPC 摄像头固件 demo。

## 功能

- [x] V4L2 mmap 采集（YUYV）
- [x] GStreamer C API 录 MP4（appsrc → x264enc → mp4mux）
- [x] GStreamer C API RTSP 推流（rtspclientsink → MediaMTX）
- [ ] MPP 硬编（可选）
- [x] 帧率统计、断流检测
- [x] HTTP API + Web 设备管理页（录像控制已接入 GStreamer）

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
│   ├── gst_recorder.c
│   ├── mediamtx_manager.c
│   ├── app_state.c
│   ├── main.c
│   ├── monitor.c
│   └── http_server.c
├── web/
│   ├── index.html
│   └── app.js
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

需要 GStreamer 开发包（`install_deps.sh` 已包含）：

```bash
sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

```bash
# 板子上 native 编译
make

# Ubuntu 18.04 虚拟机交叉编译
make cross
# 或
make CC=aarch64-linux-gnu-gcc PKG_CONFIG=aarch64-linux-gnu-pkg-config
```

### 3. 运行 V4L2 采帧

```bash
./capture -d /dev/video10 -w 640 -h 480 -n 100 -o test.yuv
# 默认已是 /dev/video10，可省略 -d
./capture -w 640 -h 480 -n 100 -o test.yuv
```

### 4. GStreamer 录 MP4（C 主链路）

V4L2 mmap 采帧 → `appsrc` → `x264enc` → `mp4mux` → MP4 文件。

```bash
# 命令行直接录像
./capture -r test.mp4

# 采 300 帧并录像
./capture -n 300 -r /tmp/test.mp4

# HTTP 控制录像（默认写到 /tmp/ipc_demo.mp4）
export IPC_DEMO_RECORD_PATH=/tmp/manual.mp4
./capture
# 浏览器 http://<板子IP>:8080/ 点「开始录像」
```

脚本方式仍可用于对照调试：

```bash
./scripts/gst_record.sh /dev/video10 test.mp4
```

### 5. RTSP 推流（C 主链路，阶段 B）

单进程：`V4L2 mmap` → `appsrc` → `x264enc` → `rtspclientsink` → **MediaMTX** → VLC。

```bash
# 启动采集 + RTSP 推流（自动拉起 MediaMTX）
./capture --stream

# 同时录像 + 推流（tee 一分二）
./capture --stream -r /tmp/test.mp4

# HTTP 控制推流
./capture
curl -X POST http://127.0.0.1:8080/stream/start
# PC VLC: rtsp://<板子IP>:8554/live
curl -X POST http://127.0.0.1:8080/stream/stop
```

环境变量：`RTSP_PATH`（默认 `live`）、`RTSP_PORT`（默认 `8554`）、`IPC_DEMO_SCRIPT_DIR`（scripts 目录）

脚本方式仍可用于对照调试：

```bash
./scripts/rtsp_push.sh /dev/video10
```

### 6. 浏览器看状态（设备管理页）

在项目根目录运行（需能访问 `web/` 目录）：

```bash
./capture
# 浏览器打开: http://<板子IP>:8080/
```

若从其他目录运行，可指定 web 目录：

```bash
export IPC_DEMO_WEB_DIR=/path/to/rk3568-ipc-demo/web
./capture
```

页面功能：fps、运行时长、断流状态、录像控制。实时画面请用 VLC + RTSP。

## 架构

预览走 RTSP，管理走 HTTP，与商用 IPC 架构一致。详见 [docs/architecture.md](docs/architecture.md)。

## 工作流

```
Windows 写代码 → git commit → git push → GitHub
板子 SSH 登录 → git pull → make → 运行测试
```
