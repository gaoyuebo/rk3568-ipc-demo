#include "gst_recorder.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gst_recorder_wait_eos(GstElement *pipeline)
{
    GstBus *bus;
    GstMessage *msg;

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND,
                                     GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
    if (!msg) {
        fprintf(stderr, "[gst] wait EOS timeout\n");
        gst_object_unref(bus);
        return -1;
    }

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err = NULL;
        gchar *dbg = NULL;

        gst_message_parse_error(msg, &err, &dbg);
        fprintf(stderr, "[gst] pipeline error: %s\n", err ? err->message : "unknown");
        if (dbg) {
            fprintf(stderr, "[gst] debug: %s\n", dbg);
        }
        g_clear_error(&err);
        g_free(dbg);
        gst_message_unref(msg);
        gst_object_unref(bus);
        return -1;
    }

    gst_message_unref(msg);
    gst_object_unref(bus);
    return 0;
}

static void gst_recorder_clear_locked(gst_recorder_t *rec)
{
    GstElement *pipeline = (GstElement *)rec->pipeline;

    if (!pipeline) {
        rec->appsrc = NULL;
        rec->active = 0;
        rec->recording = 0;
        rec->streaming = 0;
        rec->record_path[0] = '\0';
        rec->rtsp_url[0] = '\0';
        return;
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    rec->pipeline = NULL;
    rec->appsrc = NULL;
    rec->active = 0;
    rec->recording = 0;
    rec->streaming = 0;
    rec->record_path[0] = '\0';
    rec->rtsp_url[0] = '\0';
}

static int gst_recorder_build_pipeline_locked(gst_recorder_t *rec,
                                              const char *record_path,
                                              const char *rtsp_url)
{
    char pipeline_desc[1024];
    char encode_part[256];
    GstCaps *caps;
    GError *err = NULL;
    GstState state;
    GstStateChangeReturn ret;

    snprintf(encode_part, sizeof(encode_part),
             "videoconvert ! "
             "x264enc tune=zerolatency speed-preset=ultrafast bitrate=600 key-int-max=%d",
             rec->fps);

    /* h264parse + mp4mux faststart: finalize writes moov so Windows players work */
    if (record_path && rtsp_url) {
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "appsrc name=recsrc is-live=true format=time do-timestamp=true ! "
                 "%s ! tee name=t ! "
                 "queue ! h264parse ! mp4mux faststart=true ! "
                 "filesink location=\"%s\" sync=false "
                 "t. ! queue ! h264parse ! video/x-h264,stream-format=byte-stream,profile=baseline ! "
                 "rtspclientsink name=rssink location=\"%s\" protocols=tcp",
                 encode_part, record_path, rtsp_url);
    } else if (record_path) {
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "appsrc name=recsrc is-live=true format=time do-timestamp=true ! "
                 "%s ! h264parse ! mp4mux faststart=true ! "
                 "filesink location=\"%s\" sync=false",
                 encode_part, record_path);
    } else if (rtsp_url) {
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "appsrc name=recsrc is-live=true format=time do-timestamp=true ! "
                 "%s ! h264parse ! video/x-h264,stream-format=byte-stream,profile=baseline ! "
                 "rtspclientsink name=rssink location=\"%s\" protocols=tcp",
                 encode_part, rtsp_url);
    } else {
        return -1;
    }

    rec->pipeline = gst_parse_launch(pipeline_desc, &err);
    if (!rec->pipeline) {
        fprintf(stderr, "[gst] parse pipeline failed: %s\n",
                err ? err->message : "unknown");
        g_clear_error(&err);
        return -1;
    }

    rec->appsrc = gst_bin_get_by_name(GST_BIN(rec->pipeline), "recsrc");
    if (!rec->appsrc) {
        fprintf(stderr, "[gst] appsrc not found\n");
        gst_recorder_clear_locked(rec);
        return -1;
    }

    caps = gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "YUY2",
                               "width", G_TYPE_INT, rec->width,
                               "height", G_TYPE_INT, rec->height,
                               "framerate", GST_TYPE_FRACTION, rec->fps, 1,
                               NULL);
    g_object_set(G_OBJECT(rec->appsrc),
                 "caps", caps,
                 "format", GST_FORMAT_TIME,
                 "is-live", TRUE,
                 "do-timestamp", TRUE,
                 NULL);
    gst_caps_unref(caps);

    ret = gst_element_set_state((GstElement *)rec->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fprintf(stderr, "[gst] failed to set PLAYING\n");
        gst_recorder_clear_locked(rec);
        return -1;
    }

    ret = gst_element_get_state((GstElement *)rec->pipeline, &state, NULL,
                                5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE || state != GST_STATE_PLAYING) {
        fprintf(stderr, "[gst] pipeline not playing\n");
        gst_recorder_clear_locked(rec);
        return -1;
    }

    if (record_path) {
        snprintf(rec->record_path, sizeof(rec->record_path), "%s", record_path);
        rec->recording = 1;
    }
    if (rtsp_url) {
        snprintf(rec->rtsp_url, sizeof(rec->rtsp_url), "%s", rtsp_url);
        rec->streaming = 1;
    }
    rec->active = 1;
    return 0;
}

int gst_recorder_init(gst_recorder_t *rec)
{
    static int gst_inited;
    GError *err = NULL;

    memset(rec, 0, sizeof(*rec));
    pthread_mutex_init(&rec->lock, NULL);

    if (!gst_inited) {
        if (!gst_init_check(NULL, NULL, &err)) {
            fprintf(stderr, "[gst] init failed: %s\n", err ? err->message : "unknown");
            g_clear_error(&err);
            return -1;
        }
        gst_inited = 1;
    }

    return 0;
}

void gst_recorder_deinit(gst_recorder_t *rec)
{
    if (!rec) {
        return;
    }

    gst_recorder_stop(rec);
    pthread_mutex_destroy(&rec->lock);
}

int gst_recorder_apply(gst_recorder_t *rec,
                       const char *record_path,
                       const char *rtsp_url,
                       int width, int height, int fps)
{
    if (!rec || width <= 0 || height <= 0 || fps <= 0) {
        return -1;
    }

    if (!record_path && !rtsp_url) {
        return gst_recorder_stop(rec);
    }

    /* Rebuild must EOS-finalize first, or an in-progress MP4 has no moov atom. */
    pthread_mutex_lock(&rec->lock);
    if (rec->active) {
        pthread_mutex_unlock(&rec->lock);
        if (gst_recorder_stop(rec) < 0) {
            fprintf(stderr, "[gst] finalize previous pipeline failed\n");
        }
        pthread_mutex_lock(&rec->lock);
    }

    rec->width = width;
    rec->height = height;
    rec->fps = fps;

    if (gst_recorder_build_pipeline_locked(rec, record_path, rtsp_url) < 0) {
        pthread_mutex_unlock(&rec->lock);
        return -1;
    }

    if (rec->recording && rec->streaming) {
        fprintf(stderr, "[gst] record+stream started: %s -> %s %dx%d@%dfps\n",
                rec->record_path, rec->rtsp_url, width, height, fps);
    } else if (rec->recording) {
        fprintf(stderr, "[gst] recording started: %s %dx%d@%dfps\n",
                rec->record_path, width, height, fps);
    } else if (rec->streaming) {
        fprintf(stderr, "[gst] streaming started: %s %dx%d@%dfps\n",
                rec->rtsp_url, width, height, fps);
    }

    pthread_mutex_unlock(&rec->lock);
    return 0;
}

int gst_recorder_start(gst_recorder_t *rec, const char *path,
                       int width, int height, int fps)
{
    return gst_recorder_apply(rec, path, NULL, width, height, fps);
}

int gst_recorder_start_stream(gst_recorder_t *rec, const char *rtsp_url,
                              int width, int height, int fps)
{
    return gst_recorder_apply(rec, NULL, rtsp_url, width, height, fps);
}

int gst_recorder_push_frame(gst_recorder_t *rec, const void *data, size_t size)
{
    GstBuffer *buffer;
    GstFlowReturn flow;
    GstMapInfo map;
    int ret = 0;

    if (!rec || !data || size == 0) {
        return -1;
    }

    pthread_mutex_lock(&rec->lock);
    if (!rec->active || !rec->appsrc) {
        pthread_mutex_unlock(&rec->lock);
        return 0;
    }

    buffer = gst_buffer_new_allocate(NULL, size, NULL);
    if (!buffer) {
        fprintf(stderr, "[gst] alloc buffer failed\n");
        pthread_mutex_unlock(&rec->lock);
        return -1;
    }

    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        fprintf(stderr, "[gst] map buffer failed\n");
        gst_buffer_unref(buffer);
        pthread_mutex_unlock(&rec->lock);
        return -1;
    }

    memcpy(map.data, data, size);
    gst_buffer_unmap(buffer, &map);

    flow = gst_app_src_push_buffer(GST_APP_SRC(rec->appsrc), buffer);
    if (flow != GST_FLOW_OK) {
        fprintf(stderr, "[gst] push buffer failed: %s\n", gst_flow_get_name(flow));
        ret = -1;
    }

    pthread_mutex_unlock(&rec->lock);
    return ret;
}

int gst_recorder_stop(gst_recorder_t *rec)
{
    GstElement *pipeline;
    GstElement *appsrc;
    int ret = 0;

    if (!rec) {
        return -1;
    }

    pthread_mutex_lock(&rec->lock);
    if (!rec->active || !rec->pipeline) {
        pthread_mutex_unlock(&rec->lock);
        return 0;
    }

    pipeline = (GstElement *)rec->pipeline;
    appsrc = (GstElement *)rec->appsrc;
    gst_object_ref(pipeline);

    if (appsrc) {
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
    }
    /* Stop accepting new frames after EOS so mp4mux can finalize cleanly. */
    rec->active = 0;

    pthread_mutex_unlock(&rec->lock);

    if (gst_recorder_wait_eos(pipeline) < 0) {
        fprintf(stderr, "[gst] EOS wait failed, forcing pipeline down\n");
        ret = -1;
    }

    pthread_mutex_lock(&rec->lock);
    if (rec->pipeline == pipeline) {
        gst_recorder_clear_locked(rec);
    }
    pthread_mutex_unlock(&rec->lock);

    gst_object_unref(pipeline);

    fprintf(stderr, "[gst] pipeline stopped%s\n",
            ret < 0 ? " (finalize may be incomplete)" : "");
    return ret;
}

int gst_recorder_is_active(gst_recorder_t *rec)
{
    int active;

    if (!rec) {
        return 0;
    }

    pthread_mutex_lock(&rec->lock);
    active = rec->active;
    pthread_mutex_unlock(&rec->lock);
    return active;
}

int gst_recorder_is_recording(gst_recorder_t *rec)
{
    int v;

    if (!rec) {
        return 0;
    }

    pthread_mutex_lock(&rec->lock);
    v = rec->recording;
    pthread_mutex_unlock(&rec->lock);
    return v;
}

int gst_recorder_is_streaming(gst_recorder_t *rec)
{
    int v;

    if (!rec) {
        return 0;
    }

    pthread_mutex_lock(&rec->lock);
    v = rec->streaming;
    pthread_mutex_unlock(&rec->lock);
    return v;
}
