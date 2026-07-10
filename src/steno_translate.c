#include "steno_internal.h"

#include "steno_stroke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TRANSLATION_COMPACT_INTERVAL_STROKES = 1000,
    TRANSLATION_HISTORY_STROKE_LIMIT = 1000,
};

typedef enum Trace_Stroke_Mode {
    TRACE_STROKE_NORMAL,
    TRACE_STROKE_PHRASE,
    TRACE_STROKE_PHASE_FALLBACK,
} Trace_Stroke_Mode;

static const char *trace_stroke_mode_label(Trace_Stroke_Mode mode)
{
    switch (mode) {
    case TRACE_STROKE_PHRASE:
        return " [phrase]";
    case TRACE_STROKE_PHASE_FALLBACK:
        return " [phase fallback]";
    case TRACE_STROKE_NORMAL:
    default:
        return "";
    }
}

static void trace_stroke_with_mode(
    const Steno *steno,
    const char *raw_chord,
    const char *translation,
    Trace_Stroke_Mode mode
)
{
    if (steno == NULL || steno->trace_file == NULL || raw_chord == NULL) {
        return;
    }

    const char *label = trace_stroke_mode_label(mode);
    if (translation != NULL) {
        fprintf(steno->trace_file, "%s%s -> %s\n", raw_chord, label, translation);
    } else {
        fprintf(steno->trace_file, "%s%s -> [untranslated]\n", raw_chord, label);
    }
    fflush(steno->trace_file);
}

static size_t effective_lookup_stroke_limit(const Steno *steno)
{
    return translation_match_lookup_stroke_limit(steno == NULL ? NULL : &steno->dictionary_stack.dictionary);
}

static void count_completed_stroke(Steno *steno)
{
    ++steno->strokes_since_compaction;
    if (steno->strokes_since_compaction >= TRANSLATION_COMPACT_INTERVAL_STROKES) {
        size_t keep_strokes = TRANSLATION_HISTORY_STROKE_LIMIT;
        const size_t lookup_strokes = effective_lookup_stroke_limit(steno);
        if (keep_strokes < lookup_strokes) {
            keep_strokes = lookup_strokes;
        }
        translation_history_compact(&steno->translations, keep_strokes);
        steno->strokes_since_compaction = 0;
    }
}

static Phrase_Lookup_Mode phrase_lookup_mode_from_steno_mode(Steno_Phrase_Mode mode)
{
    switch (mode) {
    case STENO_PHRASE_MODE_VERBS:
        return PHRASE_LOOKUP_VERBS;
    case STENO_PHRASE_MODE_NONVERBS:
        return PHRASE_LOOKUP_NONVERBS;
    case STENO_PHRASE_MODE_ALL:
    case STENO_PHRASE_MODE_NONE:
    default:
        return PHRASE_LOOKUP_ALL;
    }
}

static Steno_Phrase_Mode normalize_stroke_phrase_mode(Stroke_Input stroke, Steno_Phrase_Mode current_mode)
{
    if (stroke.phrase_mode != STENO_PHRASE_MODE_NONE) {
        return stroke.phrase_mode;
    }
    if (stroke.phrase) {
        return STENO_PHRASE_MODE_ALL;
    }
    return current_mode;
}

static bool translate_phrase_bits(Steno *steno, uint64_t bits, Steno_Phrase_Mode phrase_mode, bool *out_hit)
{
    if (out_hit != NULL) {
        *out_hit = false;
    }
    if (bits == 0) {
        return true;
    }

    char raw_chord[64] = {0};
    if (!chord_bits_to_string(bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }

    char *phrase_text = NULL;
    const Phrase_Lookup_Result result = phrasing_lookup_mode(
        steno->phrasing,
        bits,
        phrase_lookup_mode_from_steno_mode(phrase_mode),
        &phrase_text
    );
    if (result == PHRASE_LOOKUP_ERROR) {
        free(phrase_text);
        return false;
    }
    if (result == PHRASE_LOOKUP_MISS) {
        free(phrase_text);
        return true;
    }

    Translation_Match match = {0};
    match.translation = phrase_text;
    match.strokes[0] = bits;
    match.stroke_count = 1;
    match.replaced_count = 0;
    snprintf(match.outline, sizeof(match.outline), "%s", raw_chord);

    trace_stroke_with_mode(steno, raw_chord, match.translation, TRACE_STROKE_PHRASE);
    const bool ok = steno_apply_translation_match(steno, &match);
    free(phrase_text);
    if (ok) {
        if (out_hit != NULL) {
            *out_hit = true;
        }
        count_completed_stroke(steno);
    }
    return ok;
}

static bool translate_raw_chord_bits_with_trace(
    Steno *steno,
    uint64_t bits,
    Trace_Stroke_Mode trace_mode
)
{
    char raw_chord[64] = {0};
    if (!chord_bits_to_string(bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }

    Translation_Match match = {0};
    match.translation = NULL;
    match.strokes[0] = bits;
    match.stroke_count = 1;
    match.replaced_count = 0;
    snprintf(match.outline, sizeof(match.outline), "%s", raw_chord);

    trace_stroke_with_mode(steno, raw_chord, match.translation, trace_mode);
    const bool ok = steno_apply_translation_match(steno, &match);
    if (ok) {
        count_completed_stroke(steno);
    }
    return ok;
}

static bool translate_dictionary_bits_with_trace(
    Steno *steno,
    uint64_t bits,
    Trace_Stroke_Mode trace_mode
)
{
    if (bits == 0) {
        return true;
    }

    char raw_chord[64] = {0};
    if (!chord_bits_to_string(bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }

    const char *single_stroke_translation = dictionary_lookup_bits(&steno->dictionary_stack.dictionary, bits);
    if (single_stroke_translation != NULL && single_stroke_translation[0] == '=') {
        trace_stroke_with_mode(steno, raw_chord, single_stroke_translation, trace_mode);
        const bool ok = steno_execute_command(steno, single_stroke_translation, &bits, 1);
        if (ok) {
            count_completed_stroke(steno);
        }
        return ok;
    }

    Translation_Match match = {0};
    if (!translation_match_find(&steno->dictionary_stack.dictionary, steno->translations, bits, &match)) {
        return false;
    }

    trace_stroke_with_mode(steno, match.outline, match.translation, trace_mode);
    const bool ok = steno_apply_translation_match(steno, &match);
    if (ok) {
        if (match.translation != NULL) {
            steno_maybe_emit_brevity_suggestion(steno);
        }
        count_completed_stroke(steno);
    }
    translation_match_destroy(&match);
    return ok;
}

static bool phrase_namespace_should_fallback_to_dictionary(uint64_t bits)
{
    const uint64_t star_bits = steno_bit(STENO_STAR);
    const uint64_t allowed_bits = star_bits | steno_bit(STENO_NUM);
    return (bits & star_bits) != 0 && (bits & ~allowed_bits) == 0;
}

static bool translate_phrase_namespace_bits(Steno *steno, uint64_t bits, Steno_Phrase_Mode phrase_mode)
{
    if (bits == 0) {
        return true;
    }

    bool phrase_hit = false;
    if (!translate_phrase_bits(steno, bits, phrase_mode, &phrase_hit)) {
        return false;
    }
    if (phrase_hit) {
        return true;
    }

    if (phrase_namespace_should_fallback_to_dictionary(bits)) {
        return translate_dictionary_bits_with_trace(steno, bits, TRACE_STROKE_PHASE_FALLBACK);
    }

    return translate_raw_chord_bits_with_trace(steno, bits, TRACE_STROKE_PHRASE);
}

bool steno_translate_chord_bits(Steno *steno, uint64_t bits)
{
    return translate_dictionary_bits_with_trace(steno, bits, TRACE_STROKE_NORMAL);
}

bool steno_translate_stroke_input(Steno *steno, Stroke_Input stroke)
{
    const bool phrase_namespace = stroke.phrase_namespace || steno->phrase_namespace_enabled;
    if (!phrase_namespace) {
        return steno_translate_chord_bits(steno, stroke.bits);
    }

    const Steno_Phrase_Mode phrase_mode = normalize_stroke_phrase_mode(stroke, steno->phrase_mode);
    if (phrase_mode != STENO_PHRASE_MODE_NONE) {
        return translate_phrase_namespace_bits(steno, stroke.bits, phrase_mode);
    }
    return translate_dictionary_bits_with_trace(steno, stroke.bits, TRACE_STROKE_NORMAL);
}

