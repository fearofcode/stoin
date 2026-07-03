#include "phrasing.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../stb_ds.h"

static bool append_word(char **out, const char *word)
{
    if (out == NULL || word == NULL || word[0] == '\0') {
        return true;
    }

    if (*out != NULL && (*out)[0] != '\0' && !text_append_char(out, ' ')) {
        return false;
    }
    return text_append_cstring(out, word);
}

static Phrase_Lookup_Result copy_phrase_words(const char *first, const char *second, char **out_utf8)
{
    char *result = NULL;
    if (!append_word(&result, first) || !append_word(&result, second)) {
        arrfree(result);
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = copy_cstring(result);
    arrfree(result);
    if (*out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    return PHRASE_LOOKUP_HIT;
}

static const char *iv_object_for_bits(uint64_t bits)
{
    const uint64_t r = steno_bit(STENO_RIGHT_R);
    const uint64_t p = steno_bit(STENO_RIGHT_P);
    const uint64_t b = steno_bit(STENO_RIGHT_B);
    const uint64_t t = steno_bit(STENO_RIGHT_T);

    if (bits == b) {
        return "a";
    }
    if (bits == t) {
        return "the";
    }
    if (bits == p) {
        return "it";
    }
    if (bits == (r | t)) {
        return "that";
    }
    return NULL;
}

static Phrase_Lookup_Result lookup_initial_verb(uint64_t bits, char **out_utf8)
{
    const uint64_t left_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W)
        | steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R);
    const uint64_t be_stem = steno_bit(STENO_LEFT_P) | steno_bit(STENO_LEFT_W);
    const uint64_t have_stem = steno_bit(STENO_LEFT_H);
    const uint64_t base_flag = steno_bit(STENO_E);
    const uint64_t past_flag = steno_bit(STENO_RIGHT_D);

    const uint64_t stem_bits = bits & left_mask;
    const uint64_t right_bits = bits & ~left_mask;
    const uint64_t flags = right_bits & (base_flag | past_flag);
    const uint64_t object_bits = right_bits & ~(base_flag | past_flag);
    const char *object = iv_object_for_bits(object_bits);
    if (object == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    const char *verb = NULL;
    if (stem_bits == be_stem) {
        if (flags == 0) {
            verb = "is";
        } else if (flags == past_flag) {
            verb = "was";
        } else if (flags == base_flag) {
            verb = "are";
        } else if (flags == (base_flag | past_flag)) {
            verb = "were";
        }
    } else if (stem_bits == have_stem) {
        if (flags == 0) {
            verb = "has";
        } else if (flags == past_flag) {
            verb = "had";
        } else if (flags == base_flag) {
            verb = "have";
        }
    }
    if (verb == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    return copy_phrase_words(verb, object, out_utf8);
}

static const char *nv_tail_for_bits(uint64_t bits, uint32_t allowed)
{
    enum {
        NV_TAIL_A = 1u << 0,
        NV_TAIL_THE = 1u << 1,
        NV_TAIL_THEM = 1u << 2,
        NV_TAIL_THAT = 1u << 3,
    };

    const uint64_t r = steno_bit(STENO_RIGHT_R);
    const uint64_t p = steno_bit(STENO_RIGHT_P);
    const uint64_t b = steno_bit(STENO_RIGHT_B);
    const uint64_t l = steno_bit(STENO_RIGHT_L);
    const uint64_t t = steno_bit(STENO_RIGHT_T);

    if ((allowed & NV_TAIL_A) != 0 && bits == b) {
        return "a";
    }
    if ((allowed & NV_TAIL_THE) != 0 && bits == t) {
        return "the";
    }
    if ((allowed & NV_TAIL_THEM) != 0 && bits == (p | l | t)) {
        return "them";
    }
    if ((allowed & NV_TAIL_THAT) != 0 && bits == (r | t)) {
        return "that";
    }
    return NULL;
}

static Phrase_Lookup_Result lookup_nonverb(uint64_t bits, char **out_utf8)
{
    enum {
        NV_TAIL_A = 1u << 0,
        NV_TAIL_THE = 1u << 1,
        NV_TAIL_THEM = 1u << 2,
        NV_TAIL_THAT = 1u << 3,
    };

    const uint64_t left_star_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W)
        | steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R)
        | steno_bit(STENO_STAR);
    const uint64_t k = steno_bit(STENO_LEFT_K);
    const uint64_t p = steno_bit(STENO_LEFT_P);
    const uint64_t w = steno_bit(STENO_LEFT_W);
    const uint64_t h = steno_bit(STENO_LEFT_H);
    const uint64_t r = steno_bit(STENO_LEFT_R);
    const uint64_t star = steno_bit(STENO_STAR);

    const struct {
        uint64_t bits;
        const char *word;
        uint32_t allowed_tails;
    } prefixes[] = {
        { w | h | r | star, "with", NV_TAIL_A | NV_TAIL_THE | NV_TAIL_THEM | NV_TAIL_THAT },
        { p | h | r | star, "anything", NV_TAIL_THAT },
        { k | p | h | r | star, "even", NV_TAIL_A | NV_TAIL_THAT },
    };

    const uint64_t prefix_bits = bits & left_star_mask;
    const uint64_t tail_bits = bits & ~left_star_mask;
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (prefix_bits != prefixes[i].bits) {
            continue;
        }
        const char *tail = nv_tail_for_bits(tail_bits, prefixes[i].allowed_tails);
        if (tail == NULL) {
            return PHRASE_LOOKUP_MISS;
        }
        return copy_phrase_words(prefixes[i].word, tail, out_utf8);
    }
    return PHRASE_LOOKUP_MISS;
}

typedef enum Fv_Starter_Id {
    FV_STARTER_I,
    FV_STARTER_YOU,
    FV_STARTER_HE,
    FV_STARTER_SHE,
    FV_STARTER_IT,
    FV_STARTER_WE,
    FV_STARTER_THEY,
} Fv_Starter_Id;

typedef enum Fv_Agreement {
    FV_AGREEMENT_FIRST_SINGULAR,
    FV_AGREEMENT_THIRD_SINGULAR,
    FV_AGREEMENT_PLURAL,
} Fv_Agreement;

typedef struct Fv_Starter {
    Fv_Starter_Id id;
    Fv_Agreement agreement;
    const char *text;
} Fv_Starter;

typedef enum Fv_Modal {
    FV_MODAL_NONE,
    FV_MODAL_CAN,
    FV_MODAL_SHOULD,
    FV_MODAL_WILL,
} Fv_Modal;

typedef struct Fv_Operator {
    Fv_Modal modal;
    bool negative;
} Fv_Operator;

typedef enum Fv_Structure {
    FV_STRUCTURE_SIMPLE,
    FV_STRUCTURE_PROGRESSIVE,
    FV_STRUCTURE_PERFECT,
    FV_STRUCTURE_PERFECT_PROGRESSIVE,
} Fv_Structure;

typedef enum Fv_Verb_Id {
    FV_VERB_NONE,
    FV_VERB_BE,
    FV_VERB_HAVE,
    FV_VERB_DO,
    FV_VERB_GO,
    FV_VERB_KNOW,
    FV_VERB_THINK,
    FV_VERB_WANT,
    FV_VERB_NEED,
    FV_VERB_SEE,
    FV_VERB_SAY,
    FV_VERB_GET,
    FV_VERB_FIND,
    FV_VERB_TRY,
} Fv_Verb_Id;

typedef struct Fv_Verb {
    Fv_Verb_Id id;
    const char *base;
    const char *third;
    const char *past;
    const char *present_participle;
    const char *past_participle;
} Fv_Verb;

typedef struct Fv_Ender {
    const Fv_Verb *verb;
    const char *suffix;
    bool past;
} Fv_Ender;

static const Fv_Verb FV_VERBS[] = {
    { FV_VERB_NONE, NULL, NULL, NULL, NULL, NULL },
    { FV_VERB_BE, "be", "is", "was", "being", "been" },
    { FV_VERB_HAVE, "have", "has", "had", "having", "had" },
    { FV_VERB_DO, "do", "does", "did", "doing", "done" },
    { FV_VERB_GO, "go", "goes", "went", "going", "gone" },
    { FV_VERB_KNOW, "know", "knows", "knew", "knowing", "known" },
    { FV_VERB_THINK, "think", "thinks", "thought", "thinking", "thought" },
    { FV_VERB_WANT, "want", "wants", "wanted", "wanting", "wanted" },
    { FV_VERB_NEED, "need", "needs", "needed", "needing", "needed" },
    { FV_VERB_SEE, "see", "sees", "saw", "seeing", "seen" },
    { FV_VERB_SAY, "say", "says", "said", "saying", "said" },
    { FV_VERB_GET, "get", "gets", "got", "getting", "gotten" },
    { FV_VERB_FIND, "find", "finds", "found", "finding", "found" },
    { FV_VERB_TRY, "try", "tries", "tried", "trying", "tried" },
};

static const Fv_Verb *fv_verb(Fv_Verb_Id id)
{
    for (size_t i = 0; i < sizeof(FV_VERBS) / sizeof(FV_VERBS[0]); ++i) {
        if (FV_VERBS[i].id == id) {
            return &FV_VERBS[i];
        }
    }
    return &FV_VERBS[0];
}

static bool fv_starter_lookup(uint64_t bits, Fv_Starter *out_starter)
{
    const uint64_t s = steno_bit(STENO_LEFT_S);
    const uint64_t t = steno_bit(STENO_LEFT_T);
    const uint64_t k = steno_bit(STENO_LEFT_K);
    const uint64_t p = steno_bit(STENO_LEFT_P);
    const uint64_t w = steno_bit(STENO_LEFT_W);
    const uint64_t h = steno_bit(STENO_LEFT_H);
    const uint64_t r = steno_bit(STENO_LEFT_R);

    if (bits == (s | w | r)) {
        *out_starter = (Fv_Starter){ FV_STARTER_I, FV_AGREEMENT_FIRST_SINGULAR, "I" };
        return true;
    }
    if (bits == (k | p | w | r)) {
        *out_starter = (Fv_Starter){ FV_STARTER_YOU, FV_AGREEMENT_PLURAL, "you" };
        return true;
    }
    if (bits == (k | w | h | r)) {
        *out_starter = (Fv_Starter){ FV_STARTER_HE, FV_AGREEMENT_THIRD_SINGULAR, "he" };
        return true;
    }
    if (bits == (s | k | w | h | r)) {
        *out_starter = (Fv_Starter){ FV_STARTER_SHE, FV_AGREEMENT_THIRD_SINGULAR, "she" };
        return true;
    }
    if (bits == (k | p | w | h)) {
        *out_starter = (Fv_Starter){ FV_STARTER_IT, FV_AGREEMENT_THIRD_SINGULAR, "it" };
        return true;
    }
    if (bits == (t | w | r)) {
        *out_starter = (Fv_Starter){ FV_STARTER_WE, FV_AGREEMENT_PLURAL, "we" };
        return true;
    }
    if (bits == (t | w | h)) {
        *out_starter = (Fv_Starter){ FV_STARTER_THEY, FV_AGREEMENT_PLURAL, "they" };
        return true;
    }
    return false;
}

static bool fv_operator_lookup(uint64_t bits, Fv_Operator *out_operator)
{
    const uint64_t a = steno_bit(STENO_A);
    const uint64_t o = steno_bit(STENO_O);
    const uint64_t star = steno_bit(STENO_STAR);

    if (bits == 0) {
        *out_operator = (Fv_Operator){ FV_MODAL_NONE, false };
        return true;
    }
    if (bits == star) {
        *out_operator = (Fv_Operator){ FV_MODAL_NONE, true };
        return true;
    }
    if (bits == a) {
        *out_operator = (Fv_Operator){ FV_MODAL_CAN, false };
        return true;
    }
    if (bits == (a | star)) {
        *out_operator = (Fv_Operator){ FV_MODAL_CAN, true };
        return true;
    }
    if (bits == o) {
        *out_operator = (Fv_Operator){ FV_MODAL_SHOULD, false };
        return true;
    }
    if (bits == (o | star)) {
        *out_operator = (Fv_Operator){ FV_MODAL_SHOULD, true };
        return true;
    }
    if (bits == (a | o)) {
        *out_operator = (Fv_Operator){ FV_MODAL_WILL, false };
        return true;
    }
    if (bits == (a | o | star)) {
        *out_operator = (Fv_Operator){ FV_MODAL_WILL, true };
        return true;
    }
    return false;
}

static bool fv_structure_lookup(uint64_t bits, Fv_Structure *out_structure)
{
    const uint64_t e = steno_bit(STENO_E);
    const uint64_t f = steno_bit(STENO_RIGHT_F);

    if (bits == 0) {
        *out_structure = FV_STRUCTURE_SIMPLE;
        return true;
    }
    if (bits == e) {
        *out_structure = FV_STRUCTURE_PROGRESSIVE;
        return true;
    }
    if (bits == f) {
        *out_structure = FV_STRUCTURE_PERFECT;
        return true;
    }
    if (bits == (e | f)) {
        *out_structure = FV_STRUCTURE_PERFECT_PROGRESSIVE;
        return true;
    }
    return false;
}

typedef struct Fv_Ender_Row {
    const char *stroke;
    Fv_Verb_Id verb_id;
    const char *suffix;
    bool past;
} Fv_Ender_Row;

static bool fv_ender_lookup(uint64_t bits, Fv_Ender *out_ender)
{
    static const Fv_Ender_Row rows[] = {
        { "", FV_VERB_NONE, NULL, false },
        { "D", FV_VERB_NONE, NULL, true },
        { "B", FV_VERB_BE, NULL, false },
        { "BD", FV_VERB_BE, NULL, true },
        { "BT", FV_VERB_BE, "a", false },
        { "BTD", FV_VERB_BE, "a", true },
        { "T", FV_VERB_HAVE, NULL, false },
        { "TD", FV_VERB_HAVE, NULL, true },
        { "TS", FV_VERB_HAVE, "to", false },
        { "TSDZ", FV_VERB_HAVE, "to", true },
        { "RP", FV_VERB_DO, NULL, false },
        { "RPD", FV_VERB_DO, NULL, true },
        { "RPT", FV_VERB_DO, "it", false },
        { "RPTD", FV_VERB_DO, "it", true },
        { "G", FV_VERB_GO, NULL, false },
        { "GD", FV_VERB_GO, NULL, true },
        { "GT", FV_VERB_GO, "to", false },
        { "GTD", FV_VERB_GO, "to", true },
        { "PB", FV_VERB_KNOW, NULL, false },
        { "PBD", FV_VERB_KNOW, NULL, true },
        { "PBT", FV_VERB_KNOW, "that", false },
        { "PBTD", FV_VERB_KNOW, "that", true },
        { "PBG", FV_VERB_THINK, NULL, false },
        { "PBGD", FV_VERB_THINK, NULL, true },
        { "PBGT", FV_VERB_THINK, "that", false },
        { "PBGTD", FV_VERB_THINK, "that", true },
        { "P", FV_VERB_WANT, NULL, false },
        { "PD", FV_VERB_WANT, NULL, true },
        { "PT", FV_VERB_WANT, "to", false },
        { "PTD", FV_VERB_WANT, "to", true },
        { "RPG", FV_VERB_NEED, NULL, false },
        { "RPGD", FV_VERB_NEED, NULL, true },
        { "RPGT", FV_VERB_NEED, "to", false },
        { "RPGTD", FV_VERB_NEED, "to", true },
        { "S", FV_VERB_SEE, NULL, false },
        { "SZ", FV_VERB_SEE, NULL, true },
        { "BS", FV_VERB_SAY, NULL, false },
        { "BSZ", FV_VERB_SAY, NULL, true },
        { "BTS", FV_VERB_SAY, "that", false },
        { "BTSDZ", FV_VERB_SAY, "that", true },
        { "GS", FV_VERB_GET, NULL, false },
        { "GSZ", FV_VERB_GET, NULL, true },
        { "GTS", FV_VERB_GET, "to", false },
        { "GTSDZ", FV_VERB_GET, "to", true },
        { "PBLG", FV_VERB_FIND, NULL, false },
        { "PBLGD", FV_VERB_FIND, NULL, true },
        { "PBLGT", FV_VERB_FIND, "that", false },
        { "PBLGTD", FV_VERB_FIND, "that", true },
        { "RT", FV_VERB_TRY, NULL, false },
        { "RTD", FV_VERB_TRY, NULL, true },
        { "RTS", FV_VERB_TRY, "to", false },
        { "RTSDZ", FV_VERB_TRY, "to", true },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        uint64_t row_bits = 0;
        if (rows[i].stroke[0] != '\0') {
            char outline[16] = {0};
            if (snprintf(outline, sizeof(outline), "-%s", rows[i].stroke) <= 0
                || !stroke_string_to_bits(outline, &row_bits)) {
                return false;
            }
        }

        if (bits == row_bits) {
            *out_ender = (Fv_Ender){
                .verb = fv_verb(rows[i].verb_id),
                .suffix = rows[i].suffix,
                .past = rows[i].past,
            };
            return true;
        }
    }
    return false;
}

static const char *fv_be_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return starter->agreement == FV_AGREEMENT_PLURAL ? "were" : "was";
    }
    if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR) {
        return "am";
    }
    return starter->agreement == FV_AGREEMENT_PLURAL ? "are" : "is";
}

static const char *fv_have_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "had";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "has" : "have";
}

static const char *fv_do_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "did";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "does" : "do";
}

static const char *fv_finite_verb_word(const Fv_Starter *starter, const Fv_Verb *verb, bool past)
{
    if (verb->id == FV_VERB_BE) {
        return fv_be_word(starter, past);
    }
    if (verb->id == FV_VERB_HAVE) {
        return fv_have_word(starter, past);
    }
    if (verb->id == FV_VERB_DO) {
        return fv_do_word(starter, past);
    }
    if (past) {
        return verb->past;
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? verb->third : verb->base;
}

static const char *fv_modal_word(Fv_Modal modal, bool past, bool negative)
{
    switch (modal) {
    case FV_MODAL_CAN:
        if (negative) {
            return past ? "could not" : "cannot";
        }
        return past ? "could" : "can";
    case FV_MODAL_SHOULD:
        return negative ? "should not" : "should";
    case FV_MODAL_WILL:
        if (negative) {
            return past ? "would not" : "will not";
        }
        return past ? "would" : "will";
    case FV_MODAL_NONE:
    default:
        return "";
    }
}

static const char *fv_modal_negative_contraction(Fv_Modal modal, bool past)
{
    switch (modal) {
    case FV_MODAL_CAN:
        return past ? "couldn't" : "can't";
    case FV_MODAL_SHOULD:
        return "shouldn't";
    case FV_MODAL_WILL:
        return past ? "wouldn't" : "won't";
    case FV_MODAL_NONE:
    default:
        return NULL;
    }
}

static const char *fv_will_contraction(const Fv_Starter *starter)
{
    switch (starter->id) {
    case FV_STARTER_I: return "I'll";
    case FV_STARTER_YOU: return "you'll";
    case FV_STARTER_HE: return "he'll";
    case FV_STARTER_SHE: return "she'll";
    case FV_STARTER_IT: return "it'll";
    case FV_STARTER_WE: return "we'll";
    case FV_STARTER_THEY: return "they'll";
    default: return NULL;
    }
}

static const char *fv_be_contraction(const Fv_Starter *starter)
{
    switch (starter->id) {
    case FV_STARTER_I: return "I'm";
    case FV_STARTER_YOU: return "you're";
    case FV_STARTER_HE: return "he's";
    case FV_STARTER_SHE: return "she's";
    case FV_STARTER_IT: return "it's";
    case FV_STARTER_WE: return "we're";
    case FV_STARTER_THEY: return "they're";
    default: return NULL;
    }
}

static const char *fv_have_contraction(const Fv_Starter *starter)
{
    switch (starter->id) {
    case FV_STARTER_I: return "I've";
    case FV_STARTER_YOU: return "you've";
    case FV_STARTER_HE: return "he's";
    case FV_STARTER_SHE: return "she's";
    case FV_STARTER_IT: return "it's";
    case FV_STARTER_WE: return "we've";
    case FV_STARTER_THEY: return "they've";
    default: return NULL;
    }
}

static const char *fv_be_negative_contraction(const Fv_Starter *starter, bool past)
{
    if (past) {
        return starter->agreement == FV_AGREEMENT_PLURAL ? "weren't" : "wasn't";
    }
    if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR) {
        return NULL;
    }
    return starter->agreement == FV_AGREEMENT_PLURAL ? "aren't" : "isn't";
}

static const char *fv_have_negative_contraction(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "hadn't";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "hasn't" : "haven't";
}

static bool append_verb_and_suffix(char **text, const char *verb, const char *suffix)
{
    return append_word(text, verb) && append_word(text, suffix);
}

static bool append_modal_complement(char **text, Fv_Structure structure, const Fv_Ender *ender)
{
    const bool has_verb = ender->verb->id != FV_VERB_NONE;
    switch (structure) {
    case FV_STRUCTURE_SIMPLE:
        return !has_verb || append_verb_and_suffix(text, ender->verb->base, ender->suffix);
    case FV_STRUCTURE_PROGRESSIVE:
        return append_word(text, "be")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT:
        return append_word(text, "have")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->past_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT_PROGRESSIVE:
        return append_word(text, "have")
            && append_word(text, "been")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    default:
        return false;
    }
}

static bool build_fv_long(
    const Fv_Starter *starter,
    Fv_Operator operator,
    Fv_Structure structure,
    const Fv_Ender *ender,
    char **out
)
{
    const bool has_verb = ender->verb->id != FV_VERB_NONE;
    if (operator.modal == FV_MODAL_NONE && structure == FV_STRUCTURE_SIMPLE && !has_verb) {
        return false;
    }

    if (!append_word(out, starter->text)) {
        return false;
    }

    if (operator.modal != FV_MODAL_NONE) {
        return append_word(out, fv_modal_word(operator.modal, ender->past, operator.negative))
            && append_modal_complement(out, structure, ender);
    }

    switch (structure) {
    case FV_STRUCTURE_SIMPLE:
        if (operator.negative && ender->verb->id != FV_VERB_BE) {
            return append_word(out, fv_do_word(starter, ender->past))
                && append_word(out, "not")
                && append_verb_and_suffix(out, ender->verb->base, ender->suffix);
        }
        return append_word(out, fv_finite_verb_word(starter, ender->verb, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && append_word(out, ender->suffix);
    case FV_STRUCTURE_PROGRESSIVE:
        return append_word(out, fv_be_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && (!has_verb || append_verb_and_suffix(out, ender->verb->present_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT:
        return append_word(out, fv_have_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && (!has_verb || append_verb_and_suffix(out, ender->verb->past_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT_PROGRESSIVE:
        return append_word(out, fv_have_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && append_word(out, "been")
            && (!has_verb || append_verb_and_suffix(out, ender->verb->present_participle, ender->suffix));
    default:
        return false;
    }
}

static bool append_be_contraction_complement(
    char **text,
    Fv_Structure structure,
    const Fv_Ender *ender
)
{
    const bool has_verb = ender->verb->id != FV_VERB_NONE;
    if (structure == FV_STRUCTURE_SIMPLE && ender->verb->id == FV_VERB_BE) {
        return append_word(text, ender->suffix);
    }
    if (structure == FV_STRUCTURE_PROGRESSIVE) {
        return !has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix);
    }
    return false;
}

static bool append_have_contraction_complement(
    char **text,
    Fv_Structure structure,
    const Fv_Ender *ender
)
{
    const bool has_verb = ender->verb->id != FV_VERB_NONE;
    if (structure == FV_STRUCTURE_PERFECT) {
        return !has_verb || append_verb_and_suffix(text, ender->verb->past_participle, ender->suffix);
    }
    if (structure == FV_STRUCTURE_PERFECT_PROGRESSIVE) {
        return append_word(text, "been")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    }
    return false;
}

static bool build_fv_contraction(
    const Fv_Starter *starter,
    Fv_Operator operator,
    Fv_Structure structure,
    const Fv_Ender *ender,
    char **out
)
{
    if (operator.modal != FV_MODAL_NONE) {
        if (operator.negative) {
            const char *modal = fv_modal_negative_contraction(operator.modal, ender->past);
            return modal != NULL
                && append_word(out, starter->text)
                && append_word(out, modal)
                && append_modal_complement(out, structure, ender);
        }
        if (operator.modal == FV_MODAL_WILL && !ender->past) {
            const char *contracted = fv_will_contraction(starter);
            return contracted != NULL
                && append_word(out, contracted)
                && append_modal_complement(out, structure, ender);
        }
        return false;
    }

    if (operator.negative
        && (structure == FV_STRUCTURE_PROGRESSIVE
            || (structure == FV_STRUCTURE_SIMPLE && ender->verb->id == FV_VERB_BE))) {
        if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR && !ender->past) {
            return append_word(out, fv_be_contraction(starter))
                && append_word(out, "not")
                && append_be_contraction_complement(out, structure, ender);
        }
        const char *negative = fv_be_negative_contraction(starter, ender->past);
        return negative != NULL
            && append_word(out, starter->text)
            && append_word(out, negative)
            && append_be_contraction_complement(out, structure, ender);
    }

    if (!operator.negative
        && !ender->past
        && (structure == FV_STRUCTURE_PROGRESSIVE
            || (structure == FV_STRUCTURE_SIMPLE && ender->verb->id == FV_VERB_BE))) {
        const char *contracted = fv_be_contraction(starter);
        return contracted != NULL
            && append_word(out, contracted)
            && append_be_contraction_complement(out, structure, ender);
    }

    if ((structure == FV_STRUCTURE_PERFECT || structure == FV_STRUCTURE_PERFECT_PROGRESSIVE)) {
        if (operator.negative) {
            const char *negative = fv_have_negative_contraction(starter, ender->past);
            return negative != NULL
                && append_word(out, starter->text)
                && append_word(out, negative)
                && append_have_contraction_complement(out, structure, ender);
        }
        if (!ender->past) {
            const char *contracted = fv_have_contraction(starter);
            return contracted != NULL
                && append_word(out, contracted)
                && append_have_contraction_complement(out, structure, ender);
        }
    }

    return false;
}

static Phrase_Lookup_Result lookup_final_verb(uint64_t bits, char **out_utf8)
{
    const uint64_t number = steno_bit(STENO_NUM);
    const uint64_t left_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W)
        | steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R);
    const uint64_t operator_mask = steno_bit(STENO_A)
        | steno_bit(STENO_O)
        | steno_bit(STENO_STAR);
    const uint64_t structure_mask = steno_bit(STENO_E)
        | steno_bit(STENO_RIGHT_F);
    const uint64_t ender_mask = steno_bit(STENO_RIGHT_R)
        | steno_bit(STENO_RIGHT_P)
        | steno_bit(STENO_RIGHT_B)
        | steno_bit(STENO_RIGHT_L)
        | steno_bit(STENO_RIGHT_G)
        | steno_bit(STENO_RIGHT_T)
        | steno_bit(STENO_RIGHT_S)
        | steno_bit(STENO_RIGHT_D)
        | steno_bit(STENO_RIGHT_Z);
    const uint64_t allowed = number | left_mask | operator_mask | structure_mask | ender_mask;
    if ((bits & ~allowed) != 0) {
        return PHRASE_LOOKUP_MISS;
    }

    const bool contraction = (bits & number) != 0;
    Fv_Starter starter = {0};
    Fv_Operator operator = {0};
    Fv_Structure structure = FV_STRUCTURE_SIMPLE;
    Fv_Ender ender = {0};

    if (!fv_starter_lookup(bits & left_mask, &starter)
        || !fv_operator_lookup(bits & operator_mask, &operator)
        || !fv_structure_lookup(bits & structure_mask, &structure)
        || !fv_ender_lookup(bits & ender_mask, &ender)) {
        return PHRASE_LOOKUP_MISS;
    }

    char *text = NULL;
    const bool ok = contraction
        ? build_fv_contraction(&starter, operator, structure, &ender, &text)
        : build_fv_long(&starter, operator, structure, &ender, &text);
    if (!ok || text == NULL || text[0] == '\0') {
        arrfree(text);
        return PHRASE_LOOKUP_MISS;
    }

    *out_utf8 = copy_cstring(text);
    arrfree(text);
    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
}

Phrase_Lookup_Result phrasing_lookup(uint64_t stroke_bits, char **out_utf8)
{
    if (out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = NULL;

    Phrase_Lookup_Result result = lookup_initial_verb(stroke_bits, out_utf8);
    if (result != PHRASE_LOOKUP_MISS) {
        return result;
    }

    result = lookup_nonverb(stroke_bits, out_utf8);
    if (result != PHRASE_LOOKUP_MISS) {
        return result;
    }

    return lookup_final_verb(stroke_bits, out_utf8);
}
