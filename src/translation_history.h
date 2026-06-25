#ifndef TRANSLATION_HISTORY_H
#define TRANSLATION_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "format.h"

typedef struct Translation Translation;
struct Translation {
    uint64_t *strokes;
    char *utf8;
    Translation *replaced;
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
