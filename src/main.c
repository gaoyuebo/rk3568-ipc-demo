#include "app_state.h"
#include "v4l2_capture.h"
#include "http_server.h"

#include <getopt.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile int g_running = 1;
static app_state_t g_app;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
    g_app.monitor.running = 0;
    http_server_stop(&g_app);
}

static void *video_pipeline_thread(void *arg)
{
    v4l2_capture_t *cap = arg;
    void *frame = NULL;
    size_t size = 0;
    FILE *out = NULL;
    const char *out_path = getenv("IPC_DEMO_YUV_OUT");

    if (out_path) {
        out = fopen(out_path, "wb");
        if (!out) {
            perror("fopen output");
        }
    }

    while (g_running) {
        int ret = v4l2_capture_read_frame(cap, &frame, &size);

        if (ret < 0) {
            monitor_log("[pipeline] read frame failed, retry...");
            sleep(1);
            continue;
        }

        if (ret > 0) {
            usleep(10000);
            continue;
        }

        monitor_on_frame(&g_app.monitor);

        if (gst_recorder_is_active(&g_app.recorder)) {
            if (gst_recorder_push_frame(&g_app.recorder, frame, size) < 0) {
                monitor_log("[pipeline] gst push failed");
            }
        }

        if (out && frame && size > 0) {
            fwrite(frame, 1, size, out);
        }
    }

    if (out) {
        fclose(out);
    }

    return NULL;
}

static void app_init_defaults(app_state_t *app)
{
    const char *record_env = getenv("IPC_DEMO_RECORD_PATH");
    const char *rtsp_path_env = getenv("RTSP_PATH");
    const char *rtsp_port_env = getenv("RTSP_PORT");

    snprintf(app->record_path, sizeof(app->record_path), "%s",
             record_env && record_env[0] ? record_env : APP_RECORD_PATH_DEFAULT);
    snprintf(app->rtsp_path, sizeof(app->rtsp_path), "%s",
             rtsp_path_env && rtsp_path_env[0] ? rtsp_path_env : APP_RTSP_PATH_DEFAULT);
    app->rtsp_port = rtsp_port_env && rtsp_port_env[0] ? atoi(rtsp_port_env) : APP_DEFAULT_RTSP_PORT;
    app_build_rtsp_push_url(app);
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -d <device>   V4L2 device (default: /dev/video10)\n"
            "  -w <width>    frame width (default: 640)\n"
            "  -h <height>   frame height (default: 480)\n"
            "  -f <fps>      frame rate (default: 15)\n"
            "  -n <count>    capture frame count, 0 = infinite (default: 0)\n"
            "  -o <file>     save YUV to file (optional)\n"
            "  -r <file>     record MP4 via GStreamer\n"
            "  --stream      start RTSP push to MediaMTX (rtsp://<ip>:8554/live)\n"
            "  --no-http     disable HTTP API server\n",
            prog);
}

int main(int argc, char *argv[])
{
    const char *device = "/dev/video10";
    const char *record_path = NULL;
    int width = 640;
    int height = 480;
    int frame_limit = 0;
    int enable_http = 1;
    int enable_stream = 0;
    v4l2_capture_t cap;
    pthread_t pipeline_tid;
    pthread_t monitor_tid;
    pthread_t http_tid;
    int opt;

    struct option long_opts[] = {
        {"stream", no_argument, NULL, 1001},
        {"no-http", no_argument, NULL, 1000},
        {0, 0, 0, 0}
    };

    app_init_defaults(&g_app);

    while ((opt = getopt_long(argc, argv, "d:w:h:f:n:o:r:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            device = optarg;
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'f':
            g_app.video_fps = atoi(optarg);
            break;
        case 'n':
            frame_limit = atoi(optarg);
            break;
        case 'o':
            setenv("IPC_DEMO_YUV_OUT", optarg, 1);
            break;
        case 'r':
            record_path = optarg;
            break;
        case 1001:
            enable_stream = 1;
            break;
        case 1000:
            enable_http = 0;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (g_app.video_fps <= 0) {
        const char *fps_env = getenv("IPC_DEMO_FPS");

        if (fps_env && fps_env[0]) {
            g_app.video_fps = atoi(fps_env);
        }
        if (g_app.video_fps <= 0) {
            g_app.video_fps = APP_DEFAULT_FPS;
        }
    }

    if (record_path) {
        snprintf(g_app.record_path, sizeof(g_app.record_path), "%s", record_path);
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    monitor_init(&g_app.monitor);

    if (gst_recorder_init(&g_app.recorder) < 0) {
        fprintf(stderr, "GStreamer init failed. Install dev packages:\n");
        fprintf(stderr, "  sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev\n");
        fprintf(stderr, "  sudo apt install -y gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly\n");
        return 1;
    }

    if (mediamtx_manager_init(&g_app.mediamtx, g_app.rtsp_port) < 0) {
        fprintf(stderr, "MediaMTX init failed. Run: ./scripts/install_mediamtx.sh\n");
        gst_recorder_deinit(&g_app.recorder);
        return 1;
    }

    if (v4l2_capture_open(&cap, device, width, height, V4L2_PIX_FMT_YUYV) < 0) {
        gst_recorder_deinit(&g_app.recorder);
        return 1;
    }

    if (v4l2_capture_start(&cap) < 0) {
        v4l2_capture_close(&cap);
        gst_recorder_deinit(&g_app.recorder);
        return 1;
    }

    g_app.video_width = cap.width;
    g_app.video_height = cap.height;

    monitor_log("[main] capture started: %s %dx%d YUYV", device, cap.width, cap.height);

    if (record_path) {
        if (app_start_recording(&g_app) < 0) {
            v4l2_capture_close(&cap);
            gst_recorder_deinit(&g_app.recorder);
            return 1;
        }
    }

    if (enable_stream) {
        if (app_start_streaming(&g_app) < 0) {
            v4l2_capture_close(&cap);
            gst_recorder_stop(&g_app.recorder);
            mediamtx_manager_stop(&g_app.mediamtx);
            gst_recorder_deinit(&g_app.recorder);
            return 1;
        }
        monitor_log("[main] RTSP push: %s (VLC: rtsp://<board-ip>:%d/%s)",
                    g_app.rtsp_push_url, g_app.rtsp_port, g_app.rtsp_path);
    }

    pthread_create(&monitor_tid, NULL, monitor_thread, &g_app.monitor);
    pthread_create(&pipeline_tid, NULL, video_pipeline_thread, &cap);

    if (enable_http) {
        pthread_create(&http_tid, NULL, http_server_thread, &g_app);
    }

    while (g_running) {
        if (frame_limit > 0) {
            int total;

            pthread_mutex_lock(&g_app.monitor.lock);
            total = g_app.monitor.total_frames;
            pthread_mutex_unlock(&g_app.monitor.lock);

            if (total >= frame_limit) {
                break;
            }
        }
        sleep(1);
    }

    g_running = 0;
    g_app.monitor.running = 0;
    if (enable_http) {
        http_server_stop(&g_app);
    }

    pthread_join(pipeline_tid, NULL);
    pthread_join(monitor_tid, NULL);

    if (enable_http) {
        pthread_join(http_tid, NULL);
    }

    gst_recorder_stop(&g_app.recorder);
    mediamtx_manager_stop(&g_app.mediamtx);

    pthread_mutex_lock(&g_app.monitor.lock);
    g_app.monitor.recording = 0;
    g_app.monitor.streaming = 0;
    pthread_mutex_unlock(&g_app.monitor.lock);

    v4l2_capture_close(&cap);
    gst_recorder_deinit(&g_app.recorder);
    monitor_log("[main] exit");

    return 0;
}
