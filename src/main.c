#include "v4l2_capture.h"
#include "monitor.h"
#include "http_server.h"

#include <getopt.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile int g_running = 1;
static monitor_state_t g_monitor;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
    g_monitor.running = 0;
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

        monitor_on_frame(&g_monitor);

        if (out && frame && size > 0) {
            fwrite(frame, 1, size, out);
        }
    }

    if (out) {
        fclose(out);
    }

    return NULL;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -d <device>   V4L2 device (default: /dev/video10)\n"
            "  -w <width>    frame width (default: 640)\n"
            "  -h <height>   frame height (default: 480)\n"
            "  -n <count>    capture frame count, 0 = infinite (default: 0)\n"
            "  -o <file>     save YUV to file (optional)\n"
            "  --no-http     disable HTTP API server\n",
            prog);
}

int main(int argc, char *argv[])
{
    const char *device = "/dev/video10";
    int width = 640;
    int height = 480;
    int frame_limit = 0;
    int enable_http = 1;
    v4l2_capture_t cap;
    pthread_t pipeline_tid;
    pthread_t monitor_tid;
    pthread_t http_tid;
    int opt;

    struct option long_opts[] = {
        {"no-http", no_argument, NULL, 1000},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "d:w:h:n:o:", long_opts, NULL)) != -1) {
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
        case 'n':
            frame_limit = atoi(optarg);
            break;
        case 'o':
            setenv("IPC_DEMO_YUV_OUT", optarg, 1);
            break;
        case 1000:
            enable_http = 0;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    monitor_init(&g_monitor);

    if (v4l2_capture_open(&cap, device, width, height, V4L2_PIX_FMT_YUYV) < 0) {
        return 1;
    }

    if (v4l2_capture_start(&cap) < 0) {
        v4l2_capture_close(&cap);
        return 1;
    }

    monitor_log("[main] capture started: %s %dx%d YUYV", device, cap.width, cap.height);

    pthread_create(&monitor_tid, NULL, monitor_thread, &g_monitor);
    pthread_create(&pipeline_tid, NULL, video_pipeline_thread, &cap);

    if (enable_http) {
        pthread_create(&http_tid, NULL, http_server_thread, &g_monitor);
    }

    while (g_running) {
        if (frame_limit > 0) {
            int total;

            pthread_mutex_lock(&g_monitor.lock);
            total = g_monitor.total_frames;
            pthread_mutex_unlock(&g_monitor.lock);

            if (total >= frame_limit) {
                break;
            }
        }
        sleep(1);
    }

    g_running = 0;
    g_monitor.running = 0;

    pthread_join(pipeline_tid, NULL);
    pthread_join(monitor_tid, NULL);

    if (enable_http) {
        pthread_join(http_tid, NULL);
    }

    v4l2_capture_close(&cap);
    monitor_log("[main] exit");

    return 0;
}
