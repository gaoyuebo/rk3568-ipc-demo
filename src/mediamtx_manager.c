#include "mediamtx_manager.h"

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int tcp_port_listening(int port)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int file_executable(const char *path)
{
    struct stat st;

    return path && path[0] && stat(path, &st) == 0 && (st.st_mode & S_IXUSR);
}

static void config_path_from_bin(const char *bin, char *cfg, size_t cfg_len)
{
    const char *slash = strrchr(bin, '/');

    if (slash) {
        snprintf(cfg, cfg_len, "%.*s/mediamtx.yml", (int)(slash - bin), bin);
    } else {
        snprintf(cfg, cfg_len, "scripts/mediamtx.yml");
    }
}

static int resolve_mediamtx_paths(mediamtx_manager_t *mgr)
{
    const char *env_bin = getenv("IPC_DEMO_MEDIAMTX_BIN");
    const char *env_cfg = getenv("IPC_DEMO_MEDIAMTX_CONFIG");
    const char *script_dir = getenv("IPC_DEMO_SCRIPT_DIR");
    const char *candidates[] = {
        "scripts/mediamtx",
        "/usr/bin/scripts/mediamtx",
        "/usr/bin/mediamtx",
        "./mediamtx",
        NULL
    };
    char script_bin[512];
    int i;

    if (env_bin && file_executable(env_bin)) {
        snprintf(mgr->bin_path, sizeof(mgr->bin_path), "%s", env_bin);
    }

    if (!mgr->bin_path[0] && script_dir && script_dir[0]) {
        snprintf(script_bin, sizeof(script_bin), "%s/mediamtx", script_dir);
        if (file_executable(script_bin)) {
            snprintf(mgr->bin_path, sizeof(mgr->bin_path), "%s", script_bin);
        }
    }

    if (!mgr->bin_path[0]) {
        for (i = 0; candidates[i]; i++) {
            if (file_executable(candidates[i])) {
                snprintf(mgr->bin_path, sizeof(mgr->bin_path), "%s", candidates[i]);
                break;
            }
        }
    }

    if (!mgr->bin_path[0]) {
        fprintf(stderr, "[mediamtx] binary not found, run scripts/install_mediamtx.sh\n");
        return -1;
    }

    if (env_cfg && env_cfg[0]) {
        snprintf(mgr->config_path, sizeof(mgr->config_path), "%s", env_cfg);
    } else {
        config_path_from_bin(mgr->bin_path, mgr->config_path, sizeof(mgr->config_path));
        if (access(mgr->config_path, R_OK) != 0 && script_dir && script_dir[0]) {
            snprintf(mgr->config_path, sizeof(mgr->config_path),
                     "%s/mediamtx.yml", script_dir);
        }
        if (access(mgr->config_path, R_OK) != 0) {
            snprintf(mgr->config_path, sizeof(mgr->config_path), "scripts/mediamtx.yml");
        }
    }

    if (access(mgr->config_path, R_OK) != 0) {
        fprintf(stderr, "[mediamtx] config not found: %s\n", mgr->config_path);
        return -1;
    }

    return 0;
}

int mediamtx_manager_init(mediamtx_manager_t *mgr, int port)
{
    if (!mgr) {
        return -1;
    }

    memset(mgr, 0, sizeof(*mgr));
    mgr->pid = -1;
    mgr->port = port > 0 ? port : MEDIAMTX_DEFAULT_PORT;
    return resolve_mediamtx_paths(mgr);
}

int mediamtx_manager_start(mediamtx_manager_t *mgr)
{
    pid_t pid;

    if (!mgr || !mgr->bin_path[0]) {
        return -1;
    }

    if (tcp_port_listening(mgr->port)) {
        fprintf(stderr, "[mediamtx] port %d already listening, reuse existing service\n",
                mgr->port);
        mgr->started_by_us = 0;
        mgr->pid = -1;
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[mediamtx] fork failed: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        if (access(mgr->config_path, R_OK) == 0) {
            execl(mgr->bin_path, "mediamtx", mgr->config_path, (char *)NULL);
        } else {
            execl(mgr->bin_path, "mediamtx", (char *)NULL);
        }
        fprintf(stderr, "[mediamtx] exec failed: %s\n", strerror(errno));
        _exit(127);
    }

    sleep(1);

    if (waitpid(pid, NULL, WNOHANG) == pid) {
        fprintf(stderr, "[mediamtx] process exited immediately\n");
        return -1;
    }

    if (!tcp_port_listening(mgr->port)) {
        fprintf(stderr, "[mediamtx] failed to listen on port %d\n", mgr->port);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return -1;
    }

    mgr->pid = pid;
    mgr->started_by_us = 1;
    fprintf(stderr, "[mediamtx] started pid=%d port=%d config=%s\n",
            (int)pid, mgr->port, mgr->config_path);
    return 0;
}

void mediamtx_manager_stop(mediamtx_manager_t *mgr)
{
    if (!mgr || !mgr->started_by_us || mgr->pid <= 0) {
        return;
    }

    kill(mgr->pid, SIGTERM);
    waitpid(mgr->pid, NULL, 0);
    fprintf(stderr, "[mediamtx] stopped pid=%d\n", (int)mgr->pid);
    mgr->pid = -1;
    mgr->started_by_us = 0;
}

int mediamtx_manager_is_active(const mediamtx_manager_t *mgr)
{
    if (!mgr) {
        return 0;
    }

    if (mgr->started_by_us && mgr->pid > 0 && kill(mgr->pid, 0) == 0) {
        return 1;
    }

    return tcp_port_listening(mgr->port);
}
