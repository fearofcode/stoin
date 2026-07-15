#ifndef TRANSLATION_HISTORY_H
#define TRANSLATION_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "format.h"

typedef struct Translation Translation;

typedef struct Translation_Segment_Boundary {
    char *utf8;
    size_t stroke_count;
} Translation_Segment_Boundary;

struct Translation {
    uint64_t *strokes;
    char *utf8;
    char *split_prefix_text;
    Translation_Segment_Boundary *segment_boundaries;
    Translation *replaced;
    size_t split_prefix_stroke_count;
    Case_Mode previous_case_mode;
    Case_Mode previous_next_case;
    Case_Mode resulting_case_mode;
    Case_Mode resulting_next_case;
    bool glue;
    bool next_attach;
    bool retro_space_command;
    bool has_case_state;
};

void translation_destroy(Translation *translation);
bool translation_set_strokes(Translation *translation, const uint64_t *strokes, size_t stroke_count);
char *translation_range_text(const Translation *translations, size_t start, size_t count);
char *translation_replaced_text(const Translation *translation);
char *translation_range_source_text(const Translation *translations, size_t start, size_t count);
char *translation_source_text(const Translation *translation);
size_t translation_history_stroke_count(const Translation *translations);
void translation_history_compact(Translation **translations, size_t keep_strokes);

#endif
