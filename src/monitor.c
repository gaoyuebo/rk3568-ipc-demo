#include "monitor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_PATH "/var/log/ipc_demo.log"
#define NO_FRAME_THRESHOLD 3

void monitor_init(monitor_state_t *state)
{
    memset(state, 0, sizeof(*state));
    pthread_mutex_init(&state->lock, NULL);
    state->start_time = time(NULL);
    state->http_listen_fd = -1;
    state->running = 1;
}

void monitor_on_frame(monitor_state_t *state)
{
    pthread_mutex_lock(&state->lock);
    state->frame_count++;
    state->total_frames++;
    state->no_frame_seconds = 0;
    state->stream_lost = 0;
    pthread_mutex_unlock(&state->lock);
}

void monitor_log(const char *fmt, ...)
{
    va_list ap;
    FILE *fp;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    fp = fopen(LOG_PATH, "a");
    if (!fp) {
        return;
    }

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}

void *monitor_thread(void *arg)
{
    monitor_state_t *state = arg;

    while (state->running) {
        sleep(1);

        pthread_mutex_lock(&state->lock);
        state->fps = state->frame_count;
        state->frame_count = 0;

        if (state->fps == 0) {
            state->no_frame_seconds++;
            if (state->no_frame_seconds >= NO_FRAME_THRESHOLD) {
                state->stream_lost = 1;
                monitor_log("[monitor] stream lost for %d seconds", state->no_frame_seconds);
            }
        }

        monitor_log("[monitor] fps=%d recording=%d streaming=%d stream_lost=%d",
                    state->fps, state->recording, state->streaming, state->stream_lost);
        pthread_mutex_unlock(&state->lock);
    }

    return NULL;
}
