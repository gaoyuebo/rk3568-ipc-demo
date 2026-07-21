#include "app_state.h"

#include <stdio.h>
#include <string.h>

void app_build_rtsp_push_url(app_state_t *app)
{
    snprintf(app->rtsp_push_url, sizeof(app->rtsp_push_url),
             "rtsp://127.0.0.1:%d/%s", app->rtsp_port, app->rtsp_path);
}

int app_sync_media_pipeline(app_state_t *app)
{
    const char *record_path = NULL;
    const char *rtsp_url = NULL;

    if (!app) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    if (app->monitor.recording) {
        record_path = app->record_path;
    }
    if (app->monitor.streaming) {
        rtsp_url = app->rtsp_push_url;
    }
    pthread_mutex_unlock(&app->monitor.lock);

    if (!record_path && !rtsp_url) {
        gst_recorder_stop(&app->recorder);
        return 0;
    }

    return gst_recorder_apply(&app->recorder, record_path, rtsp_url,
                              app->video_width, app->video_height,
                              app->video_fps);
}

int app_start_recording(app_state_t *app)
{
    if (!app) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    if (app->monitor.recording && gst_recorder_is_recording(&app->recorder)) {
        pthread_mutex_unlock(&app->monitor.lock);
        return 0;
    }
    app->monitor.recording = 1;
    pthread_mutex_unlock(&app->monitor.lock);

    return app_sync_media_pipeline(app);
}

int app_stop_recording(app_state_t *app)
{
    if (!app) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    app->monitor.recording = 0;
    pthread_mutex_unlock(&app->monitor.lock);

    return app_sync_media_pipeline(app);
}

int app_start_streaming(app_state_t *app)
{
    if (!app) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    if (app->monitor.streaming && gst_recorder_is_streaming(&app->recorder)) {
        pthread_mutex_unlock(&app->monitor.lock);
        return 0;
    }
    pthread_mutex_unlock(&app->monitor.lock);

    if (mediamtx_manager_start(&app->mediamtx) < 0) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    app->monitor.streaming = 1;
    pthread_mutex_unlock(&app->monitor.lock);

    return app_sync_media_pipeline(app);
}

int app_stop_streaming(app_state_t *app)
{
    if (!app) {
        return -1;
    }

    pthread_mutex_lock(&app->monitor.lock);
    app->monitor.streaming = 0;
    pthread_mutex_unlock(&app->monitor.lock);

    return app_sync_media_pipeline(app);
}
