#ifndef PHRASING_INTERNAL_H
#define PHRASING_INTERNAL_H

#include "phrasing.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../third_party/stb_ds.h"

typedef enum Fv_Agreement {
    FV_AGREEMENT_FIRST_SINGULAR,
    FV_AGREEMENT_THIRD_SINGULAR,
    FV_AGREEMENT_PLURAL,
} Fv_Agreement;

typedef enum Fv_Modal {
    FV_MODAL_NONE,
    FV_MODAL_CAN,
    FV_MODAL_SHOULD,
    FV_MODAL_WILL,
} Fv_Modal;

typedef enum Fv_Structure {
    FV_STRUCTURE_SIMPLE,
    FV_STRUCTURE_PROGRESSIVE,
    FV_STRUCTURE_PERFECT,
    FV_STRUCTURE_PERFECT_PROGRESSIVE,
} Fv_Structure;

typedef enum Fv_Verb_Kind {
    FV_VERB_OTHER,
    FV_VERB_BE,
    FV_VERB_HAVE,
    FV_VERB_DO,
} Fv_Verb_Kind;

typedef struct Phrase_Form {
    uint64_t bits;
    char *text;
} Phrase_Form;

typedef struct Iv_Stem {
    uint64_t bits;
    Phrase_Form *forms;
    size_t *tail_indices;
    bool has_tail_allowlist;
} Iv_Stem;

typedef struct Phrase_Tail {
    char *id;
    uint64_t bits;
    char *text;
    uint64_t *stem_bits;
    uint64_t *form_bits;
    bool has_stem_allowlist;
    bool has_form_allowlist;
} Phrase_Tail;

typedef struct Nv_Prefix {
    uint64_t bits;
    char *text;
    size_t *tail_indices;
} Nv_Prefix;

typedef struct Fv_Starter {
    uint64_t bits;
    Fv_Agreement agreement;
    char *text;
    char *be_contraction;
    char *have_contraction;
    char *will_contraction;
    char *d_contraction;
    size_t *ender_indices;
    bool has_ender_allowlist;
} Fv_Starter;

typedef struct Fv_Operator {
    uint64_t bits;
    Fv_Modal modal;
    bool negative;
} Fv_Operator;

typedef struct Fv_Structure_Row {
    uint64_t bits;
    Fv_Structure structure;
} Fv_Structure_Row;

typedef struct Fv_Verb {
    char *id;
    Fv_Verb_Kind kind;
    char *base;
    char *third;
    char *past;
    char *present_participle;
    char *past_participle;
} Fv_Verb;

typedef struct Fv_Ender {
    uint64_t bits;
    char *verb_id;
    const Fv_Verb *verb;
    char *suffix;
    bool past;
} Fv_Ender;

typedef struct Phrase_Suggestion {
    uint64_t bits;
    Phrase_Namespace namespace;
} Phrase_Suggestion;

typedef struct Phrase_Suggestion_Entry {
    char *key;
    Phrase_Suggestion value;
} Phrase_Suggestion_Entry;

struct Phrasing {
    Iv_Stem *iv_stems;
    Phrase_Tail *iv_tails;
    Nv_Prefix *nv_prefixes;
    Phrase_Tail *nv_tails;
    Fv_Starter *fv_starters;
    Fv_Operator *fv_operators;
    Fv_Structure_Row *fv_structures;
    Fv_Verb *fv_verbs;
    Fv_Ender *fv_enders;
    Phrase_Suggestion_Entry *suggestions;
    uint64_t contraction_bits;
    bool suggestions_initialized;
    bool suggestions_failed;
};

static inline bool phrasing_index_list_contains(const size_t *indices, size_t value)
{
    for (size_t i = 0; i < arrlenu(indices); ++i) {
        if (indices[i] == value) {
            return true;
        }
    }
    return false;
}

static inline bool phrasing_bits_list_contains(const uint64_t *bits, uint64_t value)
{
    for (size_t i = 0; i < arrlenu(bits); ++i) {
        if (bits[i] == value) {
            return true;
        }
    }
    return false;
}

static inline bool iv_combination_is_allowed(
    const Iv_Stem *stem,
    size_t tail_index,
    const Phrase_Tail *tail,
    const Phrase_Form *form
)
{
    return (!stem->has_tail_allowlist
            || phrasing_index_list_contains(stem->tail_indices, tail_index))
        && (!tail->has_stem_allowlist
            || phrasing_bits_list_contains(tail->stem_bits, stem->bits))
        && (!tail->has_form_allowlist
            || phrasing_bits_list_contains(tail->form_bits, form->bits));
}

static inline bool fv_starter_allows_ender(const Fv_Starter *starter, size_t ender_index)
{
    return !starter->has_ender_allowlist
        || phrasing_index_list_contains(starter->ender_indices, ender_index);
}

#endif
