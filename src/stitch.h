#ifndef STITCH_H
#define STITCH_H

#include "translation_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*Stitch_Replace_Output_Fn)(void *userdata, const char *old_text, const char *new_text);

typedef struct Stitch_Context {
    Translation **translations;
    Stitch_Replace_Output_Fn replace_output;
    Translation_Source source;
    void *userdata;
} Stitch_Context;

bool stitch_apply_retro(
    Stitch_Context *stitch,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count,
    size_t stitch_count,
    const char *delimiter
);

#endif
