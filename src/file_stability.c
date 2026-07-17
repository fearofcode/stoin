#include "file_stability.h"

#include "platform.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    FILE_RELOAD_POLL_MS = 10,
};

static bool file_stamps_equal(Platform_File_Stamp a, Platform_File_Stamp b)
{
    return a.exists == b.exists
        && a.size == b.size
        && a.modified_time_ns == b.modified_time_ns;
}

static bool capture_file_stamps(
    const char *const *paths,
    size_t path_count,
    Platform_File_Stamp *out_stamps
)
{
    for (size_t i = 0; i < path_count; ++i) {
        if (!platform_file_stamp(paths[i], &out_stamps[i])) {
            return false;
        }
    }
    return true;
}

bool file_paths_wait_until_stable(
    const char *const *paths,
    size_t path_count,
    bool *out_stable
)
{
    if (paths == NULL || path_count == 0 || out_stable == NULL) {
        return false;
    }

    *out_stable = false;
    Platform_File_Stamp *previous = calloc(path_count, sizeof(*previous));
    Platform_File_Stamp *current = calloc(path_count, sizeof(*current));
    if (previous == NULL || current == NULL) {
        free(previous);
        free(current);
        return false;
    }
    if (!capture_file_stamps(paths, path_count, previous)) {
        free(previous);
        free(current);
        return false;
    }

    const uint64_t started_ms = platform_monotonic_ms();
    uint64_t stable_since_ms = started_ms;
    while (true) {
        const uint64_t before_sleep_ms = platform_monotonic_ms();
        if (before_sleep_ms - stable_since_ms >= FILE_RELOAD_DEBOUNCE_MS) {
            *out_stable = true;
            break;
        }
        if (before_sleep_ms - started_ms >= FILE_RELOAD_MAX_WAIT_MS) {
            break;
        }

        platform_sleep_ms(FILE_RELOAD_POLL_MS);
        if (!capture_file_stamps(paths, path_count, current)) {
            free(previous);
            free(current);
            return false;
        }

        bool changed = false;
        for (size_t i = 0; i < path_count; ++i) {
            if (!file_stamps_equal(previous[i], current[i])) {
                changed = true;
                break;
            }
        }
        if (changed) {
            Platform_File_Stamp *swap = previous;
            previous = current;
            current = swap;
            stable_since_ms = platform_monotonic_ms();
        }
    }

    free(previous);
    free(current);
    return true;
}
