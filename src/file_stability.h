#ifndef FILE_STABILITY_H
#define FILE_STABILITY_H

#include <stdbool.h>
#include <stddef.h>

enum {
    FILE_RELOAD_DEBOUNCE_MS = 100,
    FILE_RELOAD_MAX_WAIT_MS = 2000,
};

bool file_paths_wait_until_stable(
    const char *const *paths,
    size_t path_count,
    bool *out_stable
);

#endif
