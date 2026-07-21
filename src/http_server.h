#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "app_state.h"

void *http_server_thread(void *arg);

void http_server_stop(app_state_t *app);

#endif
