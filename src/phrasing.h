#ifndef PHRASING_H
#define PHRASING_H

#include <stdint.h>

typedef enum Phrase_Namespace {
    PHRASE_NAMESPACE_NONE,
    PHRASE_NAMESPACE_INITIAL_VERB,
    PHRASE_NAMESPACE_CORE = PHRASE_NAMESPACE_INITIAL_VERB,
    PHRASE_NAMESPACE_NONVERB,
    PHRASE_NAMESPACE_CORE_OPERATOR,
} Phrase_Namespace;

typedef enum Phrase_Lookup_Result {
    PHRASE_LOOKUP_MISS,
    PHRASE_LOOKUP_HIT,
    PHRASE_LOOKUP_ERROR,
} Phrase_Lookup_Result;

Phrase_Lookup_Result phrasing_lookup(
    Phrase_Namespace namespace,
    uint64_t stroke_bits,
    char **out_utf8
);

#endif
