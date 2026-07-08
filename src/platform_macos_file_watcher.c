#include "platform_macos_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../third_party/stb_ds.h"

#ifndef O_EVTONLY
#define O_EVTONLY O_RDONLY
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
    out_stamp->modified_time_ns = (uint64_t)info.st_mtimespec.tv_sec * UINT64_C(1000000000)
        + (uint64_t)info.st_mtimespec.tv_nsec;
    return true;
}

static void clear_file_watcher_targets(void)
{
    for (size_t i = 0; i < arrlenu(g_macos.file_watcher_targets); ++i) {
        if (g_macos.file_watcher_targets[i].fd >= 0) {
            close(g_macos.file_watcher_targets[i].fd);
        }
        free(g_macos.file_watcher_targets[i].path);
    }
    arrfree(g_macos.file_watcher_targets);
    g_macos.file_watcher_targets = NULL;
}

static void clear_file_watcher_paths(void)
{
    for (size_t i = 0; i < arrlenu(g_macos.file_watcher_paths); ++i) {
        free(g_macos.file_watcher_paths[i]);
    }
    arrfree(g_macos.file_watcher_paths);
    g_macos.file_watcher_paths = NULL;
}

static bool add_file_watcher_target(const char *path)
{
    if (path == NULL || g_macos.file_watcher_kq < 0) {
        return false;
    }

    const int fd = open(path, O_EVTONLY);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return true;
        }
        return false;
    }

    struct kevent event;
    EV_SET(
        &event,
        (uintptr_t)fd,
        EVFILT_VNODE,
        EV_ADD | EV_CLEAR,
        NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_DELETE | NOTE_REVOKE,
        0,
        NULL
    );

    if (kevent(g_macos.file_watcher_kq, &event, 1, NULL, 0, NULL) != 0) {
        close(fd);
        return false;
    }

    char *path_copy = copy_cstring(path);
    if (path_copy == NULL) {
        close(fd);
        return false;
    }

    arrput(g_macos.file_watcher_targets, ((Mac_File_Watch_Target) {
        .fd = fd,
        .path = path_copy,
    }));
    return true;
}

static bool rebuild_file_watcher_targets(void)
{
    clear_file_watcher_targets();

    for (size_t i = 0; i < arrlenu(g_macos.file_watcher_paths); ++i) {
        char *parent = copy_parent_directory(g_macos.file_watcher_paths[i]);
        if (parent == NULL) {
            return false;
        }
        const bool ok = add_file_watcher_target(parent)
            && add_file_watcher_target(g_macos.file_watcher_paths[i]);
        free(parent);
        if (!ok) {
            return false;
        }
    }
    return true;
}

void platform_file_watcher_poll(void)
{
    if (!g_macos.file_watcher_active || g_macos.file_watcher_kq < 0) {
        return;
    }

    bool changed = false;
    while (true) {
        struct kevent events[16];
        const struct timespec timeout = {0};
        const int count = kevent(
            g_macos.file_watcher_kq,
            NULL,
            0,
            events,
            (int)(sizeof(events) / sizeof(events[0])),
            &timeout
        );
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (count == 0) {
            break;
        }
        changed = true;
    }

    if (!changed) {
        return;
    }

    (void)rebuild_file_watcher_targets();
    if (g_macos.file_watcher_callback != NULL) {
        g_macos.file_watcher_callback(g_macos.file_watcher_userdata);
    }
}

static void file_watcher_cf_callback(CFFileDescriptorRef descriptor, CFOptionFlags callback_types, void *info)
{
    (void)descriptor;
    (void)callback_types;
    (void)info;

    platform_file_watcher_poll();
    if (g_macos.file_watcher_descriptor != NULL) {
        CFFileDescriptorEnableCallBacks(g_macos.file_watcher_descriptor, kCFFileDescriptorReadCallBack);
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

    g_macos.file_watcher_kq = kqueue();
    if (g_macos.file_watcher_kq < 0) {
        return false;
    }
    g_macos.file_watcher_active = true;
    g_macos.file_watcher_callback = callback;
    g_macos.file_watcher_userdata = userdata;

    for (size_t i = 0; i < path_count; ++i) {
        char *copy = copy_cstring(paths[i]);
        if (copy == NULL) {
            platform_file_watcher_stop();
            return false;
        }
        arrput(g_macos.file_watcher_paths, copy);
    }

    if (!rebuild_file_watcher_targets()) {
        platform_file_watcher_stop();
        return false;
    }

    g_macos.file_watcher_run_loop = CFRunLoopGetCurrent();
    g_macos.file_watcher_descriptor = CFFileDescriptorCreate(
        kCFAllocatorDefault,
        g_macos.file_watcher_kq,
        false,
        file_watcher_cf_callback,
        NULL
    );
    if (g_macos.file_watcher_descriptor == NULL) {
        platform_file_watcher_stop();
        return false;
    }

    g_macos.file_watcher_source = CFFileDescriptorCreateRunLoopSource(
        kCFAllocatorDefault,
        g_macos.file_watcher_descriptor,
        0
    );
    if (g_macos.file_watcher_source == NULL) {
        platform_file_watcher_stop();
        return false;
    }

    CFRunLoopAddSource(g_macos.file_watcher_run_loop, g_macos.file_watcher_source, kCFRunLoopCommonModes);
    CFFileDescriptorEnableCallBacks(g_macos.file_watcher_descriptor, kCFFileDescriptorReadCallBack);
    return true;
}

void platform_file_watcher_stop(void)
{
    if (g_macos.file_watcher_source != NULL) {
        if (g_macos.file_watcher_run_loop != NULL) {
            CFRunLoopRemoveSource(
                g_macos.file_watcher_run_loop,
                g_macos.file_watcher_source,
                kCFRunLoopCommonModes
            );
        }
        CFRunLoopSourceInvalidate(g_macos.file_watcher_source);
        CFRelease(g_macos.file_watcher_source);
        g_macos.file_watcher_source = NULL;
    }

    if (g_macos.file_watcher_descriptor != NULL) {
        CFFileDescriptorInvalidate(g_macos.file_watcher_descriptor);
        CFRelease(g_macos.file_watcher_descriptor);
        g_macos.file_watcher_descriptor = NULL;
    }

    clear_file_watcher_targets();
    clear_file_watcher_paths();

    if (g_macos.file_watcher_active && g_macos.file_watcher_kq >= 0) {
        close(g_macos.file_watcher_kq);
    }
    g_macos.file_watcher_kq = -1;
    g_macos.file_watcher_active = false;
    g_macos.file_watcher_run_loop = NULL;
    g_macos.file_watcher_callback = NULL;
    g_macos.file_watcher_userdata = NULL;
}
