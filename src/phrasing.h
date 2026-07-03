#ifndef PHRASING_H
#define PHRASING_H

#include <stdint.h>

typedef enum Phrase_Lookup_Result {
    PHRASE_LOOKUP_MISS,
    PHRASE_LOOKUP_HIT,
    PHRASE_LOOKUP_ERROR,
} Phrase_Lookup_Result;

Phrase_Lookup_Result phrasing_lookup(
    uint64_t stroke_bits,
    char **out_utf8
);

#endif
