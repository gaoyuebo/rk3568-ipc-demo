#ifndef APP_STATE_H
#define APP_STATE_H

#include "gst_recorder.h"
#include "mediamtx_manager.h"
#include "monitor.h"

#define APP_RECORD_PATH_DEFAULT "/tmp/ipc_demo.mp4"
#define APP_RTSP_PATH_DEFAULT "live"
#define APP_DEFAULT_FPS 15
#define APP_DEFAULT_RTSP_PORT 8554

typedef struct {
    monitor_state_t monitor;
    gst_recorder_t recorder;
    mediamtx_manager_t mediamtx;
    int video_width;
    int video_height;
    int video_fps;
    int rtsp_port;
    char rtsp_path[64];
    char rtsp_push_url[128];
    char record_path[256];
} app_state_t;

void app_build_rtsp_push_url(app_state_t *app);

int app_sync_media_pipeline(app_state_t *app);
int app_start_recording(app_state_t *app);
int app_stop_recording(app_state_t *app);
int app_start_streaming(app_state_t *app);
int app_stop_streaming(app_state_t *app);

#endif
