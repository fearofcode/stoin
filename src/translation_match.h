#ifndef TRANSLATION_MATCH_H
#define TRANSLATION_MATCH_H

#include "dictionary.h"
#include "translation_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TRANSLATION_MATCH_MAX_STROKES = 100,
    TRANSLATION_MATCH_MAX_OUTLINE_BYTES = 4096,
};

typedef struct Translation_Match {
    const char *translation;
    const char *suffix_base_translation;
    const char *suffix_translation;
    char *owned_translation;
    uint64_t strokes[TRANSLATION_MATCH_MAX_STROKES];
    size_t stroke_count;
    size_t replaced_count;
    char outline[TRANSLATION_MATCH_MAX_OUTLINE_BYTES];
    bool suffix_match;
} Translation_Match;

size_t translation_match_lookup_stroke_limit(const Dictionary *dictionary);
bool translation_match_find(
    const Dictionary *dictionary,
    const Translation *history,
    uint64_t bits,
    Translation_Match *out_match
);
void translation_match_destroy(Translation_Match *match);

#endif
