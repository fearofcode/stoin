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
} Iv_Stem;

typedef struct Phrase_Tail {
    char *id;
    uint64_t bits;
    char *text;
} Phrase_Tail;

typedef struct Nv_Prefix {
    uint64_t bits;
    char *text;
} Nv_Prefix;

typedef struct Fv_Starter {
    uint64_t bits;
    Fv_Agreement agreement;
    char *text;
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
    bool has_explicit_verb;
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
    bool suggestions_initialized;
    bool suggestions_failed;
};

#endif
