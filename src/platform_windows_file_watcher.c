#include "platform_windows_internal.h"

#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

typedef struct Windows_File_Watch_Target {
    char *path;
    Platform_File_Stamp stamp;
} Windows_File_Watch_Target;

typedef struct Windows_File_Watcher_State {
    Platform_File_Watch_Fn file_watcher_callback;
    void *file_watcher_userdata;
    Windows_File_Watch_Target *file_watcher_targets;
    bool file_watcher_active;
} Windows_File_Watcher_State;

static Windows_File_Watcher_State g_windows;

static char *copy_nullable_cstring(const char *s)
{
    return s == NULL ? NULL : copy_cstring(s);
}

bool platform_file_stamp(const char *path, Platform_File_Stamp *out_stamp)
{
    if (path == NULL || out_stamp == NULL) {
        return false;
    }

    memset(out_stamp, 0, sizeof(*out_stamp));

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        return false;
    }

    ULARGE_INTEGER modified;
    modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
    modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
    ULARGE_INTEGER size;
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;

    out_stamp->exists = true;
    out_stamp->size = size.QuadPart;
    out_stamp->modified_time_ns = modified.QuadPart * UINT64_C(100);
    return true;
}

static bool file_stamps_equal(Platform_File_Stamp a, Platform_File_Stamp b)
{
    return a.exists == b.exists
        && a.size == b.size
        && a.modified_time_ns == b.modified_time_ns;
}

static void clear_file_watcher_targets(void)
{
    for (size_t i = 0; i < arrlenu(g_windows.file_watcher_targets); ++i) {
        free(g_windows.file_watcher_targets[i].path);
    }
    arrfree(g_windows.file_watcher_targets);
    g_windows.file_watcher_targets = NULL;
}

void platform_file_watcher_poll(void)
{
    if (!g_windows.file_watcher_active) {
        return;
    }

    bool changed = false;
    for (size_t i = 0; i < arrlenu(g_windows.file_watcher_targets); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(g_windows.file_watcher_targets[i].path, &stamp)) {
            continue;
        }
        if (!file_stamps_equal(stamp, g_windows.file_watcher_targets[i].stamp)) {
            g_windows.file_watcher_targets[i].stamp = stamp;
            changed = true;
        }
    }

    if (changed && g_windows.file_watcher_callback != NULL) {
        g_windows.file_watcher_callback(g_windows.file_watcher_userdata);
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

    for (size_t i = 0; i < path_count; ++i) {
        char *copy = copy_nullable_cstring(paths[i]);
        if (copy == NULL) {
            platform_file_watcher_stop();
            return false;
        }

        Platform_File_Stamp stamp = {0};
        (void)platform_file_stamp(copy, &stamp);
        Windows_File_Watch_Target target = {
            .path = copy,
            .stamp = stamp,
        };
        arrput(g_windows.file_watcher_targets, target);
    }

    g_windows.file_watcher_active = true;
    g_windows.file_watcher_callback = callback;
    g_windows.file_watcher_userdata = userdata;
    return true;
}

void platform_file_watcher_stop(void)
{
    clear_file_watcher_targets();
    g_windows.file_watcher_active = false;
    g_windows.file_watcher_callback = NULL;
    g_windows.file_watcher_userdata = NULL;
}
