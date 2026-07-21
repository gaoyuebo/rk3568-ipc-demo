#include "app_state.h"
#include "http_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define HTTP_PORT 8080
#define WEB_DIR_DEFAULT "web"
#define STATIC_MAX_SIZE (256 * 1024)

static void http_send_raw(int client, int code, const char *content_type,
                          const char *body, size_t body_len)
{
    char header[256];

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             code, content_type, body_len);
    send(client, header, strlen(header), 0);
    if (body && body_len > 0) {
        send(client, body, body_len, 0);
    }
}

static void http_send_json(int client, int code, const char *body)
{
    http_send_raw(client, code, "application/json", body, strlen(body));
}

static const char *web_dir_path(void)
{
    const char *dir = getenv("IPC_DEMO_WEB_DIR");

    return dir ? dir : WEB_DIR_DEFAULT;
}

static int read_static_file(const char *filename, char **out, size_t *out_len)
{
    char path[512];
    struct stat st;
    FILE *fp;
    char *buf;
    size_t nread;
    const char *dir = web_dir_path();

    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    if (stat(path, &st) < 0 || st.st_size <= 0 || (size_t)st.st_size > STATIC_MAX_SIZE) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    buf = malloc((size_t)st.st_size + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    nread = fread(buf, 1, (size_t)st.st_size, fp);
    fclose(fp);
    if (nread != (size_t)st.st_size) {
        free(buf);
        return -1;
    }

    buf[nread] = '\0';
    *out = buf;
    *out_len = nread;
    return 0;
}

static void http_send_static(int client, const char *filename, const char *content_type)
{
    char *body = NULL;
    size_t body_len = 0;

    if (read_static_file(filename, &body, &body_len) < 0) {
        http_send_json(client, 404, "{\"error\":\"not found\"}");
        return;
    }

    http_send_raw(client, 200, content_type, body, body_len);
    free(body);
}

static void handle_request(int client, app_state_t *app, const char *req)
{
    monitor_state_t *state = &app->monitor;
    char body[256];
    time_t uptime;

    /* Must use length 6 for "GET / " so "/status" and "/app.js" are not matched. */
    if (strncmp(req, "GET / ", 6) == 0 ||
        strncmp(req, "GET / HTTP", 10) == 0 ||
        strncmp(req, "GET /index.html", 15) == 0) {
        http_send_static(client, "index.html", "text/html; charset=utf-8");
        return;
    }

    if (strncmp(req, "GET /app.js", 11) == 0) {
        http_send_static(client, "app.js", "application/javascript; charset=utf-8");
        return;
    }

    if (strncmp(req, "GET /status", 11) == 0) {
        pthread_mutex_lock(&state->lock);
        uptime = time(NULL) - state->start_time;
        snprintf(body, sizeof(body),
                 "{\"fps\":%d,\"recording\":%d,\"streaming\":%d,\"uptime\":%ld,\"stream_lost\":%d}",
                 state->fps, state->recording, state->streaming,
                 (long)uptime, state->stream_lost);
        pthread_mutex_unlock(&state->lock);
        http_send_json(client, 200, body);
        return;
    }

    if (strncmp(req, "POST /record/start", 18) == 0) {
        if (app_start_recording(app) < 0) {
            http_send_json(client, 500, "{\"error\":\"start recording failed\"}");
            return;
        }

        monitor_log("[http] recording started: %s", app->record_path);
        http_send_json(client, 200, "{\"ok\":true,\"recording\":true}");
        return;
    }

    if (strncmp(req, "POST /record/stop", 17) == 0) {
        if (app_stop_recording(app) < 0) {
            http_send_json(client, 500, "{\"error\":\"stop recording failed\"}");
            return;
        }

        monitor_log("[http] recording stopped");
        http_send_json(client, 200, "{\"ok\":true,\"recording\":false}");
        return;
    }

    if (strncmp(req, "POST /stream/start", 18) == 0) {
        if (app_start_streaming(app) < 0) {
            http_send_json(client, 500, "{\"error\":\"start streaming failed\"}");
            return;
        }

        monitor_log("[http] streaming started: %s", app->rtsp_push_url);
        http_send_json(client, 200, "{\"ok\":true,\"streaming\":true}");
        return;
    }

    if (strncmp(req, "POST /stream/stop", 17) == 0) {
        if (app_stop_streaming(app) < 0) {
            http_send_json(client, 500, "{\"error\":\"stop streaming failed\"}");
            return;
        }

        monitor_log("[http] streaming stopped");
        http_send_json(client, 200, "{\"ok\":true,\"streaming\":false}");
        return;
    }

    http_send_json(client, 404, "{\"error\":\"not found\"}");
}

void http_server_stop(app_state_t *app)
{
    int fd;

    pthread_mutex_lock(&app->monitor.lock);
    fd = app->monitor.http_listen_fd;
    pthread_mutex_unlock(&app->monitor.lock);

    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
    }
}

void *http_server_thread(void *arg)
{
    app_state_t *app = arg;
    monitor_state_t *state = &app->monitor;
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

    monitor_log("[http] server listening on port %d (web dir: %s)",
                HTTP_PORT, web_dir_path());

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
            handle_request(client, app, buf);
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
