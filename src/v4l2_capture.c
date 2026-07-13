#include "v4l2_capture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>

static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);

    return ret;
}

static void v4l2_capture_unmap(v4l2_capture_t *cap)
{
    int i;

    for (i = 0; i < cap->buffer_count; i++) {
        if (cap->buffers[i] && cap->buffers[i] != MAP_FAILED) {
            munmap(cap->buffers[i], cap->buffer_sizes[i]);
        }
        cap->buffers[i] = NULL;
        cap->buffer_sizes[i] = 0;
    }
    cap->buffer_count = 0;
}

int v4l2_capture_open(v4l2_capture_t *cap, const char *device,
                      int width, int height, uint32_t pixfmt)
{
    struct v4l2_capability cap_info;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    int i;

    memset(cap, 0, sizeof(*cap));
    cap->fd = -1;

    cap->fd = open(device, O_RDWR | O_NONBLOCK, 0);
    if (cap->fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
        return -1;
    }

    if (xioctl(cap->fd, VIDIOC_QUERYCAP, &cap_info) < 0) {
        fprintf(stderr, "VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
        goto fail;
    }

    if (!(cap_info.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is not a video capture device\n", device);
        goto fail;
    }

    if (!(cap_info.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming I/O\n", device);
        goto fail;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = (unsigned int)width;
    fmt.fmt.pix.height = (unsigned int)height;
    fmt.fmt.pix.pixelformat = pixfmt;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(cap->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "VIDIOC_S_FMT failed: %s\n", strerror(errno));
        goto fail;
    }

    cap->width = (int)fmt.fmt.pix.width;
    cap->height = (int)fmt.fmt.pix.height;
    cap->pixfmt = fmt.fmt.pix.pixelformat;

    memset(&req, 0, sizeof(req));
    req.count = V4L2_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(cap->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr, "VIDIOC_REQBUFS failed: %s\n", strerror(errno));
        goto fail;
    }

    if (req.count < 2) {
        fprintf(stderr, "insufficient buffer memory\n");
        goto fail;
    }

    cap->buffer_count = (int)req.count;

    for (i = 0; i < cap->buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = (unsigned int)i;

        if (xioctl(cap->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "VIDIOC_QUERYBUF failed: %s\n", strerror(errno));
            goto fail;
        }

        cap->buffer_sizes[i] = buf.length;
        cap->buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, cap->fd, buf.m.offset);
        if (cap->buffers[i] == MAP_FAILED) {
            fprintf(stderr, "mmap failed: %s\n", strerror(errno));
            goto fail;
        }
    }

    return 0;

fail:
    v4l2_capture_close(cap);
    return -1;
}

int v4l2_capture_start(v4l2_capture_t *cap)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int i;

    for (i = 0; i < cap->buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = (unsigned int)i;

        if (xioctl(cap->fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
            return -1;
        }
    }

    if (xioctl(cap->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr, "VIDIOC_STREAMON failed: %s\n", strerror(errno));
        return -1;
    }

    cap->streaming = 1;
    return 0;
}

int v4l2_capture_read_frame(v4l2_capture_t *cap, void **data, size_t *size)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(cap->fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            return 1;
        }
        fprintf(stderr, "VIDIOC_DQBUF failed: %s\n", strerror(errno));
        return -1;
    }

    if (data) {
        *data = cap->buffers[buf.index];
    }
    if (size) {
        *size = buf.bytesused;
    }

    if (xioctl(cap->fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

void v4l2_capture_stop(v4l2_capture_t *cap)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (!cap->streaming) {
        return;
    }

    if (xioctl(cap->fd, VIDIOC_STREAMOFF, &type) < 0) {
        fprintf(stderr, "VIDIOC_STREAMOFF failed: %s\n", strerror(errno));
    }

    cap->streaming = 0;
}

void v4l2_capture_close(v4l2_capture_t *cap)
{
    if (!cap) {
        return;
    }

    v4l2_capture_stop(cap);
    v4l2_capture_unmap(cap);

    if (cap->fd >= 0) {
        close(cap->fd);
        cap->fd = -1;
    }
}
