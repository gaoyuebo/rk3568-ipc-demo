#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "monitor.h"

void *http_server_thread(void *arg);

void http_server_stop(monitor_state_t *state);

#endif
