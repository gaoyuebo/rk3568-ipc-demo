#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

#define V4L2_BUFFER_COUNT 4

typedef struct {
    int fd;
    int width;
    int height;
    uint32_t pixfmt;
    void *buffers[V4L2_BUFFER_COUNT];
    size_t buffer_sizes[V4L2_BUFFER_COUNT];
    int buffer_count;
    int streaming;
} v4l2_capture_t;

int v4l2_capture_open(v4l2_capture_t *cap, const char *device,
                      int width, int height, uint32_t pixfmt);
int v4l2_capture_start(v4l2_capture_t *cap);
int v4l2_capture_read_frame(v4l2_capture_t *cap, void **data, size_t *size);
void v4l2_capture_stop(v4l2_capture_t *cap);
void v4l2_capture_close(v4l2_capture_t *cap);

#endif
