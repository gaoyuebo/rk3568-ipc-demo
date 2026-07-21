#ifndef MEDIAMTX_MANAGER_H
#define MEDIAMTX_MANAGER_H

#include <sys/types.h>

#define MEDIAMTX_DEFAULT_PORT 8554

typedef struct {
    pid_t pid;
    int started_by_us;
    int port;
    char bin_path[512];
    char config_path[512];
} mediamtx_manager_t;

int mediamtx_manager_init(mediamtx_manager_t *mgr, int port);
int mediamtx_manager_start(mediamtx_manager_t *mgr);
void mediamtx_manager_stop(mediamtx_manager_t *mgr);
int mediamtx_manager_is_active(const mediamtx_manager_t *mgr);

#endif
