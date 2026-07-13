#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include <time.h>

typedef struct {
    pthread_mutex_t lock;
    int frame_count;
    int total_frames;
    int fps;
    int no_frame_seconds;
    int stream_lost;
    time_t start_time;
    int recording;
    volatile int running;
} monitor_state_t;

void monitor_init(monitor_state_t *state);
void monitor_on_frame(monitor_state_t *state);
void *monitor_thread(void *arg);
void monitor_log(const char *fmt, ...);

#endif
