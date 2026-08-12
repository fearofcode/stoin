#include "phrasing_internal.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

bool phrasing_decode_stroke(
    uint64_t encoded_bits,
    Phrase_Namespace *out_namespace,
    uint64_t *out_phrase_bits
)
{
    if (out_namespace == NULL || out_phrase_bits == NULL || encoded_bits == 0) {
        return false;
    }

    const uint64_t final_verb_marker = steno_bit(STENO_U);
    const uint64_t nonverb_marker = steno_bit(STENO_E);
    if ((encoded_bits & final_verb_marker) != 0) {
        *out_namespace = PHRASE_NAMESPACE_FINAL_VERB;
        *out_phrase_bits = encoded_bits & ~final_verb_marker;
    } else if ((encoded_bits & nonverb_marker) != 0) {
        *out_namespace = PHRASE_NAMESPACE_NONVERB;
        *out_phrase_bits = encoded_bits & ~nonverb_marker;
    } else {
        *out_namespace = PHRASE_NAMESPACE_INITIAL_VERB;
        *out_phrase_bits = encoded_bits;
    }
    return *out_phrase_bits != 0;
}

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
    *out_utf8 = copy_cstring(result != NULL ? result : "");
    arrfree(result);
    if (*out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    return PHRASE_LOOKUP_HIT;
}

static Phrase_Lookup_Result lookup_initial_verb(const Phrasing *phrasing, uint64_t bits, char **out_utf8)
{
    for (size_t i = 0; i < arrlenu(phrasing->iv_stems); ++i) {
        const Iv_Stem *stem = &phrasing->iv_stems[i];
        for (size_t j = 0; j < arrlenu(stem->forms); ++j) {
            const Phrase_Form *form = &stem->forms[j];
            for (size_t k = 0; k < arrlenu(phrasing->iv_tails); ++k) {
                const Phrase_Tail *tail = &phrasing->iv_tails[k];
                if (bits == (stem->bits | form->bits | tail->bits)) {
                    return copy_phrase_words(form->text, tail->text, out_utf8);
                }
            }
        }
    }
    return PHRASE_LOOKUP_MISS;
}

static Phrase_Lookup_Result lookup_nonverb(const Phrasing *phrasing, uint64_t bits, char **out_utf8)
{
    for (size_t i = 0; i < arrlenu(phrasing->nv_prefixes); ++i) {
        const Nv_Prefix *prefix = &phrasing->nv_prefixes[i];
        for (size_t j = 0; j < arrlenu(phrasing->nv_tails); ++j) {
            const Phrase_Tail *tail = &phrasing->nv_tails[j];
            if (bits == (prefix->bits | tail->bits)) {
                return copy_phrase_words(prefix->text, tail->text, out_utf8);
            }
        }
    }
    return PHRASE_LOOKUP_MISS;
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
    if (verb->kind == FV_VERB_BE) {
        return fv_be_word(starter, past);
    }
    if (verb->kind == FV_VERB_HAVE) {
        return fv_have_word(starter, past);
    }
    if (verb->kind == FV_VERB_DO) {
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

static bool append_verb_and_suffix(char **text, const char *verb, const char *suffix)
{
    return append_word(text, verb) && append_word(text, suffix);
}

static bool append_modal_complement(
    char **text,
    Fv_Structure structure,
    const Fv_Ender *ender
)
{
    const bool has_verb = ender->verb != NULL;
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
    const bool has_verb = ender->verb != NULL;
    if (operator.modal == FV_MODAL_NONE && structure == FV_STRUCTURE_SIMPLE && !has_verb) {
        return operator.negative
            && append_word(out, starter->text)
            && append_word(out, fv_do_word(starter, ender->past))
            && append_word(out, "not");
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
        if (operator.negative && ender->verb->kind != FV_VERB_BE) {
            return append_word(out, fv_do_word(starter, ender->past))
                && append_word(out, "not")
                && append_verb_and_suffix(out, ender->verb->base, ender->suffix);
        }
        return append_word(out, fv_finite_verb_word(starter, ender->verb, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && append_word(out, ender->suffix);
    case FV_STRUCTURE_PROGRESSIVE:
        return append_word(
                out,
                !has_verb && ender->past
                    ? "were"
                    : fv_be_word(starter, ender->past))
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

static Phrase_Lookup_Result lookup_final_verb(
    const Phrasing *phrasing,
    uint64_t bits,
    char **out_utf8
)
{
    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        const Fv_Starter *starter = &phrasing->fv_starters[i];
        for (size_t j = 0; j < arrlenu(phrasing->fv_operators); ++j) {
            Fv_Operator operator = phrasing->fv_operators[j];
            if (starter->requires_modal && operator.modal == FV_MODAL_NONE) {
                continue;
            }
            for (size_t k = 0; k < arrlenu(phrasing->fv_structures); ++k) {
                const Fv_Structure_Row *structure = &phrasing->fv_structures[k];
                for (size_t m = 0; m < arrlenu(phrasing->fv_enders); ++m) {
                    const Fv_Ender *ender = &phrasing->fv_enders[m];
                    const uint64_t long_bits = starter->bits | operator.bits | structure->bits | ender->bits;
                    if (bits != long_bits) {
                        continue;
                    }

                    char *text = NULL;
                    const bool ok = build_fv_long(
                        starter,
                        operator,
                        structure->structure,
                        ender,
                        &text);
                    if (!ok || text == NULL || text[0] == '\0') {
                        arrfree(text);
                        continue;
                    }

                    *out_utf8 = copy_cstring(text);
                    arrfree(text);
                    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
                }
            }
        }
    }
    return PHRASE_LOOKUP_MISS;
}

typedef struct Seen_Phrase_Bits {
    uint64_t key;
    bool value;
} Seen_Phrase_Bits;

static bool phrase_suggestion_is_better(
    Phrase_Suggestion candidate,
    Phrase_Suggestion current
)
{
    if (candidate.namespace != current.namespace) {
        return candidate.namespace < current.namespace;
    }
    if (candidate.has_explicit_verb != current.has_explicit_verb) {
        return candidate.has_explicit_verb;
    }

    char candidate_outline[64] = {0};
    char current_outline[64] = {0};
    if (!chord_bits_to_string(candidate.bits, candidate_outline, sizeof(candidate_outline))
        || !chord_bits_to_string(current.bits, current_outline, sizeof(current_outline))) {
        return false;
    }

    const size_t candidate_length = strlen(candidate_outline);
    const size_t current_length = strlen(current_outline);
    return candidate_length < current_length
        || (candidate_length == current_length
            && strcmp(candidate_outline, current_outline) < 0);
}

static bool add_phrase_suggestion(
    Phrase_Suggestion_Entry **suggestions,
    Seen_Phrase_Bits **seen_bits,
    const char *text,
    Phrase_Namespace namespace,
    uint64_t bits,
    bool has_explicit_verb
)
{
    uint64_t encoded_bits = bits;
    if (namespace == PHRASE_NAMESPACE_FINAL_VERB) {
        encoded_bits |= steno_bit(STENO_U);
    } else if (namespace == PHRASE_NAMESPACE_NONVERB) {
        encoded_bits |= steno_bit(STENO_E);
    }

    if (hmgeti(*seen_bits, encoded_bits) >= 0) {
        return true;
    }
    hmput(*seen_bits, encoded_bits, true);
    if (text == NULL || text[0] == '\0') {
        return true;
    }

    const Phrase_Suggestion candidate = {
        .bits = encoded_bits,
        .namespace = namespace,
        .has_explicit_verb = has_explicit_verb,
    };
    const ptrdiff_t index = shgeti(*suggestions, text);
    if (index < 0) {
        shput(*suggestions, text, candidate);
    } else if (phrase_suggestion_is_better(candidate, (*suggestions)[index].value)) {
        (*suggestions)[index].value = candidate;
    }
    return true;
}

static bool add_initial_verb_suggestions(Phrasing *phrasing)
{
    Seen_Phrase_Bits *seen_bits = NULL;
    for (size_t i = 0; i < arrlenu(phrasing->iv_stems); ++i) {
        const Iv_Stem *stem = &phrasing->iv_stems[i];
        for (size_t j = 0; j < arrlenu(stem->forms); ++j) {
            const Phrase_Form *form = &stem->forms[j];
            for (size_t k = 0; k < arrlenu(phrasing->iv_tails); ++k) {
                const Phrase_Tail *tail = &phrasing->iv_tails[k];
                char *text = NULL;
                const Phrase_Lookup_Result result = copy_phrase_words(form->text, tail->text, &text);
                if (result == PHRASE_LOOKUP_ERROR) {
                    hmfree(seen_bits);
                    return false;
                }
                const bool ok = add_phrase_suggestion(
                    &phrasing->suggestions,
                    &seen_bits,
                    text,
                    PHRASE_NAMESPACE_INITIAL_VERB,
                    stem->bits | form->bits | tail->bits,
                    false);
                free(text);
                if (!ok) {
                    hmfree(seen_bits);
                    return false;
                }
            }
        }
    }
    hmfree(seen_bits);
    return true;
}

static bool add_final_verb_suggestions(Phrasing *phrasing)
{
    Seen_Phrase_Bits *seen_bits = NULL;
    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        const Fv_Starter *starter = &phrasing->fv_starters[i];
        for (size_t j = 0; j < arrlenu(phrasing->fv_operators); ++j) {
            const Fv_Operator operator = phrasing->fv_operators[j];
            if (starter->requires_modal && operator.modal == FV_MODAL_NONE) {
                continue;
            }
            for (size_t k = 0; k < arrlenu(phrasing->fv_structures); ++k) {
                const Fv_Structure_Row *structure = &phrasing->fv_structures[k];
                for (size_t m = 0; m < arrlenu(phrasing->fv_enders); ++m) {
                    const Fv_Ender *ender = &phrasing->fv_enders[m];
                    const uint64_t long_bits =
                        starter->bits | operator.bits | structure->bits | ender->bits;

                    char *text = NULL;
                    if (build_fv_long(
                            starter,
                            operator,
                            structure->structure,
                            ender,
                            &text)
                        && text != NULL
                        && text[0] != '\0') {
                        add_phrase_suggestion(
                            &phrasing->suggestions,
                            &seen_bits,
                            text,
                            PHRASE_NAMESPACE_FINAL_VERB,
                            long_bits,
                            ender->verb != NULL);
                    }
                    arrfree(text);
                }
            }
        }
    }
    hmfree(seen_bits);
    return true;
}

static bool add_nonverb_suggestions(Phrasing *phrasing)
{
    Seen_Phrase_Bits *seen_bits = NULL;
    for (size_t i = 0; i < arrlenu(phrasing->nv_prefixes); ++i) {
        const Nv_Prefix *prefix = &phrasing->nv_prefixes[i];
        for (size_t j = 0; j < arrlenu(phrasing->nv_tails); ++j) {
            const Phrase_Tail *tail = &phrasing->nv_tails[j];
            char *text = NULL;
            const Phrase_Lookup_Result result = copy_phrase_words(prefix->text, tail->text, &text);
            if (result == PHRASE_LOOKUP_ERROR) {
                hmfree(seen_bits);
                return false;
            }
            const bool ok = add_phrase_suggestion(
                &phrasing->suggestions,
                &seen_bits,
                text,
                PHRASE_NAMESPACE_NONVERB,
                prefix->bits | tail->bits,
                false);
            free(text);
            if (!ok) {
                hmfree(seen_bits);
                return false;
            }
        }
    }
    hmfree(seen_bits);
    return true;
}

static bool initialize_phrase_suggestions(Phrasing *phrasing)
{
    phrasing->suggestions_initialized = true;
    sh_new_strdup(phrasing->suggestions);
    if (!add_initial_verb_suggestions(phrasing)
        || !add_final_verb_suggestions(phrasing)
        || !add_nonverb_suggestions(phrasing)) {
        shfree(phrasing->suggestions);
        phrasing->suggestions = NULL;
        phrasing->suggestions_failed = true;
        return false;
    }
    return true;
}

bool phrasing_for_each_suggestion(
    Phrasing *phrasing,
    Phrase_Suggestion_Fn callback,
    void *userdata
)
{
    if (phrasing == NULL || callback == NULL) {
        return false;
    }
    if (!phrasing->suggestions_initialized && !initialize_phrase_suggestions(phrasing)) {
        return false;
    }
    if (phrasing->suggestions_failed) {
        return false;
    }

    for (ptrdiff_t i = 0; i < shlen(phrasing->suggestions); ++i) {
        char outline[64] = {0};
        if (!chord_bits_to_string(
                phrasing->suggestions[i].value.bits,
                outline,
                sizeof(outline))
            || !callback(
                phrasing->suggestions[i].key,
                outline,
                phrasing->suggestions[i].value.namespace,
                userdata)) {
            return false;
        }
    }
    return true;
}

Phrase_Lookup_Result phrasing_find_translation_outline(
    Phrasing *phrasing,
    const char *translation,
    const char *exclude_outline,
    size_t max_stroke_count,
    Phrase_Namespace *out_namespace,
    char *out_outline,
    size_t out_outline_size
)
{
    if (out_namespace == NULL || out_outline == NULL || out_outline_size == 0) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_namespace = PHRASE_NAMESPACE_NONE;
    out_outline[0] = '\0';
    if (phrasing == NULL || translation == NULL || max_stroke_count < 1) {
        return PHRASE_LOOKUP_MISS;
    }
    if (!phrasing->suggestions_initialized && !initialize_phrase_suggestions(phrasing)) {
        return PHRASE_LOOKUP_ERROR;
    }
    if (phrasing->suggestions_failed) {
        return PHRASE_LOOKUP_ERROR;
    }

    const ptrdiff_t index = shgeti(phrasing->suggestions, translation);
    if (index < 0) {
        return PHRASE_LOOKUP_MISS;
    }

    char outline[64] = {0};
    if (!chord_bits_to_string(
            phrasing->suggestions[index].value.bits,
            outline,
            sizeof(outline))) {
        return PHRASE_LOOKUP_ERROR;
    }
    if (exclude_outline != NULL && strcmp(outline, exclude_outline) == 0) {
        return PHRASE_LOOKUP_MISS;
    }
    if (snprintf(out_outline, out_outline_size, "%s", outline) >= (int)out_outline_size) {
        out_outline[0] = '\0';
        return PHRASE_LOOKUP_ERROR;
    }

    *out_namespace = phrasing->suggestions[index].value.namespace;
    return PHRASE_LOOKUP_HIT;
}

Phrase_Lookup_Result phrasing_lookup(
    const Phrasing *phrasing,
    Phrase_Namespace namespace,
    uint64_t stroke_bits,
    char **out_utf8
)
{
    if (out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = NULL;
    if (phrasing == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    switch (namespace) {
    case PHRASE_NAMESPACE_INITIAL_VERB:
        return lookup_initial_verb(phrasing, stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_FINAL_VERB:
        return lookup_final_verb(phrasing, stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONVERB:
        return lookup_nonverb(phrasing, stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONE:
    default:
        return PHRASE_LOOKUP_MISS;
    }
}
