#include "phrasing.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../stb_ds.h"

typedef enum Phrase_Subject_Form {
    PHRASE_SUBJECT_FIRST_SINGULAR,
    PHRASE_SUBJECT_SECOND_OR_PLURAL,
    PHRASE_SUBJECT_THIRD_SINGULAR,
} Phrase_Subject_Form;

typedef struct Phrase_Starter {
    const char *text;
    Phrase_Subject_Form form;
} Phrase_Starter;

typedef struct Phrase_Verb {
    uint64_t bits;
    const char *base;
    const char *present_3ps;
    const char *past;
    const char *present_participle;
    const char *past_participle;
    bool be;
} Phrase_Verb;

typedef struct Phrase_Grammar {
    bool past;
    bool negative;
    bool inverted;
    unsigned int aux;
    unsigned int aspect;
} Phrase_Grammar;

enum {
    PHRASE_AUX_NONE = 0,
    PHRASE_AUX_CAN = 1,
    PHRASE_AUX_SHOULD = 2,
    PHRASE_AUX_WILL = 3,

    PHRASE_ASPECT_SIMPLE = 0,
    PHRASE_ASPECT_PROGRESSIVE = 1,
    PHRASE_ASPECT_PERFECT = 2,
    PHRASE_ASPECT_PERFECT_PROGRESSIVE = 3,
};

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

static bool append_words3(char **out, const char *a, const char *b, const char *c)
{
    return append_word(out, a) && append_word(out, b) && append_word(out, c);
}

static bool starter_lookup(uint64_t bits, Phrase_Starter *out)
{
    const uint64_t s = steno_bit(STENO_LEFT_S);
    const uint64_t t = steno_bit(STENO_LEFT_T);
    const uint64_t k = steno_bit(STENO_LEFT_K);
    const uint64_t p = steno_bit(STENO_LEFT_P);
    const uint64_t w = steno_bit(STENO_LEFT_W);

    if (bits == 0) {
        *out = (Phrase_Starter) { .text = "I", .form = PHRASE_SUBJECT_FIRST_SINGULAR };
        return true;
    }
    if (bits == w) {
        *out = (Phrase_Starter) { .text = "we", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL };
        return true;
    }
    if (bits == k) {
        *out = (Phrase_Starter) { .text = "he", .form = PHRASE_SUBJECT_THIRD_SINGULAR };
        return true;
    }
    if (bits == (s | k)) {
        *out = (Phrase_Starter) { .text = "she", .form = PHRASE_SUBJECT_THIRD_SINGULAR };
        return true;
    }
    if (bits == p) {
        *out = (Phrase_Starter) { .text = "it", .form = PHRASE_SUBJECT_THIRD_SINGULAR };
        return true;
    }
    if (bits == t) {
        *out = (Phrase_Starter) { .text = "they", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL };
        return true;
    }
    if (bits == (s | t)) {
        *out = (Phrase_Starter) { .text = "that", .form = PHRASE_SUBJECT_THIRD_SINGULAR };
        return true;
    }
    if (bits == (p | w)) {
        *out = (Phrase_Starter) { .text = "you", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL };
        return true;
    }
    return false;
}

static bool subject_is_3ps(Phrase_Starter starter)
{
    return starter.form == PHRASE_SUBJECT_THIRD_SINGULAR;
}

static bool subject_is_first_singular(Phrase_Starter starter)
{
    return starter.form == PHRASE_SUBJECT_FIRST_SINGULAR;
}

static bool verb_lookup(uint64_t bits, const Phrase_Verb **out)
{
    static const Phrase_Verb verbs[] = {
        {
            .bits = UINT64_C(1) << STENO_RIGHT_B,
            .base = "be",
            .present_3ps = "is",
            .past = "was",
            .present_participle = "being",
            .past_participle = "been",
            .be = true,
        },
        {
            .bits = UINT64_C(1) << STENO_RIGHT_G,
            .base = "go",
            .present_3ps = "goes",
            .past = "went",
            .present_participle = "going",
            .past_participle = "gone",
        },
        {
            .bits = (UINT64_C(1) << STENO_RIGHT_B) | (UINT64_C(1) << STENO_RIGHT_L),
            .base = "believe",
            .present_3ps = "believes",
            .past = "believed",
            .present_participle = "believing",
            .past_participle = "believed",
        },
        {
            .bits = (UINT64_C(1) << STENO_RIGHT_R)
                | (UINT64_C(1) << STENO_RIGHT_P)
                | (UINT64_C(1) << STENO_RIGHT_B),
            .base = "understand",
            .present_3ps = "understands",
            .past = "understood",
            .present_participle = "understanding",
            .past_participle = "understood",
        },
    };

    for (size_t i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i) {
        if (verbs[i].bits == bits) {
            *out = &verbs[i];
            return true;
        }
    }
    return false;
}

static const char *tail_lookup(uint64_t bits)
{
    const uint64_t t = steno_bit(STENO_RIGHT_T);
    const uint64_t s = steno_bit(STENO_RIGHT_S);
    const uint64_t d = steno_bit(STENO_RIGHT_D);
    const uint64_t z = steno_bit(STENO_RIGHT_Z);

    if (bits == 0) return "";
    if (bits == t) return "the";
    if (bits == s) return "a";
    if (bits == d) return "it";
    if (bits == z) return "that";
    if (bits == (t | s)) return "this";
    if (bits == (t | d)) return "me";
    if (bits == (t | z)) return "those";
    if (bits == (s | d)) return "her";
    if (bits == (s | z)) return "us";
    if (bits == (d | z)) return "them";
    if (bits == (t | s | d)) return "you";
    if (bits == (t | s | z)) return "these";
    if (bits == (t | d | z)) return "him";
    if (bits == (s | d | z)) return "one";
    if (bits == (t | s | d | z)) return "all";
    return NULL;
}

static const char *finite_be(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        if (subject_is_3ps(starter) || subject_is_first_singular(starter)) {
            return negative ? "wasn't" : "was";
        }
        return negative ? "weren't" : "were";
    }

    if (subject_is_first_singular(starter)) {
        return negative ? "am not" : "am";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "isn't" : "is";
    }
    return negative ? "aren't" : "are";
}

static const char *finite_have(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        return negative ? "hadn't" : "had";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "hasn't" : "has";
    }
    return negative ? "haven't" : "have";
}

static const char *do_support(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        return negative ? "didn't" : "did";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "doesn't" : "does";
    }
    return negative ? "don't" : "do";
}

static const char *aux_word(const Phrase_Grammar *grammar)
{
    switch (grammar->aux) {
    case PHRASE_AUX_CAN: return grammar->past ? "could" : "can";
    case PHRASE_AUX_SHOULD: return "should";
    case PHRASE_AUX_WILL: return grammar->past ? "would" : "will";
    default: return "";
    }
}

static const char *negative_aux_word(const Phrase_Grammar *grammar)
{
    switch (grammar->aux) {
    case PHRASE_AUX_CAN: return grammar->past ? "couldn't" : "can't";
    case PHRASE_AUX_SHOULD: return "shouldn't";
    case PHRASE_AUX_WILL: return grammar->past ? "wouldn't" : "won't";
    default: return "";
    }
}

static const char *simple_verb_form(Phrase_Starter starter, const Phrase_Verb *verb, bool past)
{
    if (verb->be) {
        return finite_be(starter, past, false);
    }
    if (past) {
        return verb->past;
    }
    return subject_is_3ps(starter) ? verb->present_3ps : verb->base;
}

static bool append_simple_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_word(out, verb->base);
    }

    if (verb->be) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative));
    }

    if (grammar->negative) {
        return append_word(out, do_support(starter, grammar->past, true))
            && append_word(out, verb->base);
    }

    return append_word(out, simple_verb_form(starter, verb, grammar->past));
}

static bool append_progressive_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "be", verb->present_participle, NULL);
    }
    return append_word(out, finite_be(starter, grammar->past, grammar->negative))
        && append_word(out, verb->present_participle);
}

static bool append_perfect_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "have", verb->past_participle, NULL);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_word(out, verb->past_participle);
}

static bool append_perfect_progressive_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "have", "been", verb->present_participle);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_words3(out, "been", verb->present_participle, NULL);
}

static bool append_bare_aux_complement(
    char **out,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    switch (grammar->aspect) {
    case PHRASE_ASPECT_SIMPLE:
        return append_word(out, verb->base);
    case PHRASE_ASPECT_PROGRESSIVE:
        return append_words3(out, "be", verb->present_participle, NULL);
    case PHRASE_ASPECT_PERFECT:
        return append_words3(out, "have", verb->past_participle, NULL);
    case PHRASE_ASPECT_PERFECT_PROGRESSIVE:
        return append_words3(out, "have", "been", verb->present_participle);
    default:
        return false;
    }
}

static bool append_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    switch (grammar->aspect) {
    case PHRASE_ASPECT_SIMPLE:
        return append_simple_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PROGRESSIVE:
        return append_progressive_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PERFECT:
        return append_perfect_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PERFECT_PROGRESSIVE:
        return append_perfect_progressive_predicate(out, starter, grammar, verb);
    default:
        return false;
    }
}

static bool append_inverted_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aspect == PHRASE_ASPECT_SIMPLE && grammar->aux == PHRASE_AUX_NONE && !verb->be) {
        return append_word(out, do_support(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->base);
    }

    if (grammar->aspect == PHRASE_ASPECT_SIMPLE && grammar->aux == PHRASE_AUX_NONE && verb->be) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text);
    }

    if (grammar->aux != PHRASE_AUX_NONE) {
        if (!append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            || !append_word(out, starter.text)) {
            return false;
        }
        return append_bare_aux_complement(out, grammar, verb);
    }

    if (grammar->aspect == PHRASE_ASPECT_PROGRESSIVE) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->present_participle);
    }
    if (grammar->aspect == PHRASE_ASPECT_PERFECT) {
        return append_word(out, finite_have(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->past_participle);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_word(out, starter.text)
        && append_word(out, "been")
        && append_word(out, verb->present_participle);
}

static bool grammar_decode(uint64_t bits, Phrase_Grammar *out)
{
    const uint64_t h = steno_bit(STENO_LEFT_H);
    const uint64_t r = steno_bit(STENO_LEFT_R);
    const uint64_t a = steno_bit(STENO_A);
    const uint64_t o = steno_bit(STENO_O);
    const uint64_t star = steno_bit(STENO_STAR);
    const uint64_t e = steno_bit(STENO_E);
    const uint64_t u = steno_bit(STENO_U);

    *out = (Phrase_Grammar) {
        .past = (bits & h) != 0,
        .negative = (bits & star) != 0,
        .inverted = (bits & u) != 0,
        .aux = 0,
        .aspect = 0,
    };

    if ((bits & a) != 0) {
        out->aux |= PHRASE_AUX_CAN;
    }
    if ((bits & o) != 0) {
        out->aux |= PHRASE_AUX_SHOULD;
    }
    if ((bits & e) != 0) {
        out->aspect |= PHRASE_ASPECT_PROGRESSIVE;
    }
    if ((bits & r) != 0) {
        out->aspect |= PHRASE_ASPECT_PERFECT;
    }
    return true;
}

static Phrase_Lookup_Result lookup_core(uint64_t bits, char **out_utf8)
{
    const uint64_t starter_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W);
    const uint64_t grammar_mask = steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R)
        | steno_bit(STENO_A)
        | steno_bit(STENO_O)
        | steno_bit(STENO_STAR)
        | steno_bit(STENO_E)
        | steno_bit(STENO_U);
    const uint64_t verb_mask = steno_bit(STENO_RIGHT_F)
        | steno_bit(STENO_RIGHT_R)
        | steno_bit(STENO_RIGHT_P)
        | steno_bit(STENO_RIGHT_B)
        | steno_bit(STENO_RIGHT_L)
        | steno_bit(STENO_RIGHT_G);
    const uint64_t tail_mask = steno_bit(STENO_RIGHT_T)
        | steno_bit(STENO_RIGHT_S)
        | steno_bit(STENO_RIGHT_D)
        | steno_bit(STENO_RIGHT_Z);

    if ((bits & ~(starter_mask | grammar_mask | verb_mask | tail_mask)) != 0) {
        return PHRASE_LOOKUP_MISS;
    }

    Phrase_Starter starter = {0};
    const uint64_t starter_bits = bits & starter_mask;
    if (!starter_lookup(starter_bits, &starter)) {
        return PHRASE_LOOKUP_MISS;
    }

    Phrase_Grammar grammar = {0};
    if (!grammar_decode(bits & grammar_mask, &grammar)) {
        return PHRASE_LOOKUP_ERROR;
    }

    const Phrase_Verb *verb = NULL;
    const uint64_t verb_bits = bits & verb_mask;
    if (!verb_lookup(verb_bits, &verb)) {
        return PHRASE_LOOKUP_MISS;
    }

    const char *tail = tail_lookup(bits & tail_mask);
    if (tail == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    char *text = NULL;
    bool ok = true;
    if (grammar.inverted) {
        ok = append_inverted_predicate(&text, starter, &grammar, verb);
    } else {
        ok = append_word(&text, starter.text)
            && append_predicate(&text, starter, &grammar, verb);
    }
    ok = ok && append_word(&text, tail);

    if (!ok || text == NULL) {
        arrfree(text);
        return PHRASE_LOOKUP_ERROR;
    }

    *out_utf8 = copy_cstring(text);
    arrfree(text);
    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
}

Phrase_Lookup_Result phrasing_lookup(
    Phrase_Namespace namespace,
    uint64_t stroke_bits,
    char **out_utf8
)
{
    if (out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = NULL;

    switch (namespace) {
    case PHRASE_NAMESPACE_CORE:
        return lookup_core(stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONE:
    case PHRASE_NAMESPACE_NONVERB:
    case PHRASE_NAMESPACE_CORE_OPERATOR:
        return PHRASE_LOOKUP_MISS;
    default:
        return PHRASE_LOOKUP_ERROR;
    }
}
