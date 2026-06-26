#include "platform_linux_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../stb_ds.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

static char *copy_range_cstring(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *copy_cstring(const char *s)
{
    return s == NULL ? NULL : copy_range_cstring(s, strlen(s));
}

static char *copy_parent_directory(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (slash == NULL) {
        return copy_cstring(".");
    }
    if (slash == path) {
        return copy_cstring("/");
    }
    return copy_range_cstring(path, (size_t)(slash - path));
}

bool platform_file_stamp(const char *path, Platform_File_Stamp *out_stamp)
{
    if (path == NULL || out_stamp == NULL) {
        return false;
    }

    memset(out_stamp, 0, sizeof(*out_stamp));

    struct stat info;
    if (stat(path, &info) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        return false;
    }

    out_stamp->exists = true;
    out_stamp->size = (uint64_t)info.st_size;
    out_stamp->modified_time_ns = (uint64_t)info.st_mtim.tv_sec * UINT64_C(1000000000)
        + (uint64_t)info.st_mtim.tv_nsec;
    return true;
}

static void clear_file_watcher_targets(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.file_watcher_targets); ++i) {
        if (g_linux.file_watcher_targets[i].wd >= 0 && g_linux.file_watcher_fd >= 0) {
            (void)inotify_rm_watch(g_linux.file_watcher_fd, g_linux.file_watcher_targets[i].wd);
        }
        free(g_linux.file_watcher_targets[i].path);
    }
    arrfree(g_linux.file_watcher_targets);
    g_linux.file_watcher_targets = NULL;
}

static void clear_file_watcher_paths(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.file_watcher_paths); ++i) {
        free(g_linux.file_watcher_paths[i]);
    }
    arrfree(g_linux.file_watcher_paths);
    g_linux.file_watcher_paths = NULL;
}

static bool add_file_watcher_target(const char *path)
{
    if (path == NULL || g_linux.file_watcher_fd < 0) {
        return false;
    }

    const uint32_t mask = IN_CLOSE_WRITE
        | IN_MODIFY
        | IN_ATTRIB
        | IN_CREATE
        | IN_DELETE
        | IN_MOVED_FROM
        | IN_MOVED_TO
        | IN_MOVE_SELF
        | IN_DELETE_SELF;
    const int wd = inotify_add_watch(g_linux.file_watcher_fd, path, mask);
    if (wd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return true;
        }
        return false;
    }

    char *path_copy = copy_cstring(path);
    if (path_copy == NULL) {
        (void)inotify_rm_watch(g_linux.file_watcher_fd, wd);
        return false;
    }

    arrput(g_linux.file_watcher_targets, ((Linux_File_Watch_Target) {
        .wd = wd,
        .path = path_copy,
    }));
    return true;
}

static bool rebuild_file_watcher_targets(void)
{
    clear_file_watcher_targets();

    for (size_t i = 0; i < arrlenu(g_linux.file_watcher_paths); ++i) {
        char *parent = copy_parent_directory(g_linux.file_watcher_paths[i]);
        if (parent == NULL) {
            return false;
        }
        const bool ok = add_file_watcher_target(parent)
            && add_file_watcher_target(g_linux.file_watcher_paths[i]);
        free(parent);
        if (!ok) {
            return false;
        }
    }
    return true;
}

int linux_file_watcher_fd(void)
{
    return g_linux.file_watcher_active ? g_linux.file_watcher_fd : -1;
}

void platform_file_watcher_poll(void)
{
    if (!g_linux.file_watcher_active || g_linux.file_watcher_fd < 0) {
        return;
    }

    bool changed = false;
    while (true) {
        char buffer[4096];
        const ssize_t bytes_read = read(g_linux.file_watcher_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            changed = true;
            continue;
        }
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }

    if (!changed) {
        return;
    }

    (void)rebuild_file_watcher_targets();
    if (g_linux.file_watcher_callback != NULL) {
        g_linux.file_watcher_callback(g_linux.file_watcher_userdata);
    }
}

bool platform_file_watcher_start(
    const char *const *paths,
    size_t path_count,
    Platform_File_Watch_Fn callback,
    void *userdata
)
{
    if (paths == NULL || path_count == 0 || callback == NULL) {
        return false;
    }

    platform_file_watcher_stop();

    g_linux.file_watcher_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (g_linux.file_watcher_fd < 0) {
        return false;
    }
    g_linux.file_watcher_active = true;
    g_linux.file_watcher_callback = callback;
    g_linux.file_watcher_userdata = userdata;

    for (size_t i = 0; i < path_count; ++i) {
        char *copy = copy_cstring(paths[i]);
        if (copy == NULL) {
            platform_file_watcher_stop();
            return false;
        }
        arrput(g_linux.file_watcher_paths, copy);
    }

    if (!rebuild_file_watcher_targets()) {
        platform_file_watcher_stop();
        return false;
    }

    return true;
}

void platform_file_watcher_stop(void)
{
    clear_file_watcher_targets();
    clear_file_watcher_paths();

    if (g_linux.file_watcher_fd >= 0) {
        close(g_linux.file_watcher_fd);
    }
    g_linux.file_watcher_fd = -1;
    g_linux.file_watcher_active = false;
    g_linux.file_watcher_callback = NULL;
    g_linux.file_watcher_userdata = NULL;
}
