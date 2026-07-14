#ifndef PHRASING_H
#define PHRASING_H

#include <stdint.h>

typedef struct Phrasing Phrasing;

typedef enum Phrase_Lookup_Result {
    PHRASE_LOOKUP_MISS,
    PHRASE_LOOKUP_HIT,
    PHRASE_LOOKUP_ERROR,
} Phrase_Lookup_Result;

typedef enum Phrase_Lookup_Mode {
    PHRASE_LOOKUP_ALL,
    PHRASE_LOOKUP_VERBS,
    PHRASE_LOOKUP_INITIAL_VERBS,
    PHRASE_LOOKUP_NONVERBS,
} Phrase_Lookup_Mode;

Phrasing *phrasing_load(const char *path);
void phrasing_destroy(Phrasing *phrasing);

Phrase_Lookup_Result phrasing_lookup(
    const Phrasing *phrasing,
    uint64_t stroke_bits,
    char **out_utf8
);
Phrase_Lookup_Result phrasing_lookup_mode(
    const Phrasing *phrasing,
    uint64_t stroke_bits,
    Phrase_Lookup_Mode mode,
    char **out_utf8
);

#endif
