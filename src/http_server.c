#include "http_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define HTTP_PORT 8080

static void http_send_response(int client, int code, const char *body)
{
    char header[256];

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             code, strlen(body));
    send(client, header, strlen(header), 0);
    send(client, body, strlen(body), 0);
}

static void handle_request(int client, monitor_state_t *state, const char *req)
{
    char body[256];
    time_t uptime;

    if (strncmp(req, "GET /status", 11) == 0) {
        pthread_mutex_lock(&state->lock);
        uptime = time(NULL) - state->start_time;
        snprintf(body, sizeof(body),
                 "{\"fps\":%d,\"recording\":%d,\"uptime\":%ld,\"stream_lost\":%d}",
                 state->fps, state->recording, (long)uptime, state->stream_lost);
        pthread_mutex_unlock(&state->lock);
        http_send_response(client, 200, body);
        return;
    }

    if (strncmp(req, "POST /record/start", 18) == 0) {
        pthread_mutex_lock(&state->lock);
        state->recording = 1;
        pthread_mutex_unlock(&state->lock);
        http_send_response(client, 200, "{\"ok\":true,\"recording\":true}");
        return;
    }

    if (strncmp(req, "POST /record/stop", 17) == 0) {
        pthread_mutex_lock(&state->lock);
        state->recording = 0;
        pthread_mutex_unlock(&state->lock);
        http_send_response(client, 200, "{\"ok\":true,\"recording\":false}");
        return;
    }

    http_send_response(client, 404, "{\"error\":\"not found\"}");
}

void http_server_stop(monitor_state_t *state)
{
    int fd;

    pthread_mutex_lock(&state->lock);
    fd = state->http_listen_fd;
    pthread_mutex_unlock(&state->lock);

    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
    }
}

void *http_server_thread(void *arg)
{
    monitor_state_t *state = arg;
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        monitor_log("[http] socket failed");
        return NULL;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(HTTP_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        monitor_log("[http] bind failed on port %d", HTTP_PORT);
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 4) < 0) {
        monitor_log("[http] listen failed");
        close(server_fd);
        return NULL;
    }

    monitor_log("[http] server listening on port %d", HTTP_PORT);

    pthread_mutex_lock(&state->lock);
    state->http_listen_fd = server_fd;
    pthread_mutex_unlock(&state->lock);

    while (state->running) {
        struct pollfd pfd = {
            .fd = server_fd,
            .events = POLLIN,
        };
        int client;
        char buf[1024];
        ssize_t n;
        int pr;

        pr = poll(&pfd, 1, 1000);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (pr == 0) {
            continue;
        }

        client = accept(server_fd, NULL, NULL);
        if (client < 0) {
            if (!state->running) {
                break;
            }
            continue;
        }

        n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            handle_request(client, state, buf);
        }

        close(client);
    }

    pthread_mutex_lock(&state->lock);
    state->http_listen_fd = -1;
    pthread_mutex_unlock(&state->lock);
    close(server_fd);
    monitor_log("[http] server stopped");
    return NULL;
}
