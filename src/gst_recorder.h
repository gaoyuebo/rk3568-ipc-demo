#ifndef GST_RECORDER_H
#define GST_RECORDER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gst_recorder {
    pthread_mutex_t lock;
    void *pipeline; /* GstElement* */
    void *appsrc;   /* GstElement* */
    int active;
    int recording;
    int streaming;
    int width;
    int height;
    int fps;
    char record_path[256];
    char rtsp_url[256];
} gst_recorder_t;

int gst_recorder_init(gst_recorder_t *rec);
void gst_recorder_deinit(gst_recorder_t *rec);

int gst_recorder_apply(gst_recorder_t *rec,
                       const char *record_path,
                       const char *rtsp_url,
                       int width, int height, int fps);

int gst_recorder_start(gst_recorder_t *rec, const char *path,
                       int width, int height, int fps);
int gst_recorder_start_stream(gst_recorder_t *rec, const char *rtsp_url,
                              int width, int height, int fps);

int gst_recorder_push_frame(gst_recorder_t *rec, const void *data, size_t size);
int gst_recorder_stop(gst_recorder_t *rec);

int gst_recorder_is_active(gst_recorder_t *rec);
int gst_recorder_is_recording(gst_recorder_t *rec);
int gst_recorder_is_streaming(gst_recorder_t *rec);

#endif
