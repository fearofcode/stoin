#include "steno_internal.h"

#include "steno_stroke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

enum {
    TRANSLATION_COMPACT_INTERVAL_STROKES = 1000,
    TRANSLATION_HISTORY_STROKE_LIMIT = 1000,
};

typedef enum Trace_Stroke_Mode {
    TRACE_STROKE_NORMAL,
    TRACE_STROKE_PHRASE,
} Trace_Stroke_Mode;

static const char *trace_stroke_mode_label(Trace_Stroke_Mode mode)
{
    switch (mode) {
    case TRACE_STROKE_PHRASE:
        return " [phrase]";
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
    if (steno == NULL) {
        return TRANSLATION_MATCH_MAX_STROKES;
    }
    return translation_match_lookup_stroke_limit(&steno->dictionary_stack.dictionary);
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

static void set_replacement_format_case_state(
    const Steno *steno,
    Translation_Match *match
)
{
    if (steno == NULL
        || match == NULL
        || match->has_format_case_state
        || match->replaced_count == 0
        || match->partial_prefix_stroke_count != 0) {
        return;
    }

    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        return;
    }

    const Translation *first = &steno->translations[
        translation_count - match->replaced_count
    ];
    while (arrlenu(first->replaced) > 0) {
        first = &first->replaced[0];
    }
    if (!first->has_case_state) {
        return;
    }

    match->format_case_mode = first->previous_case_mode;
    match->format_next_case = first->previous_next_case;
    match->has_format_case_state = true;
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
        const bool ok = steno_execute_command(
            steno,
            single_stroke_translation,
            &bits,
            1);
        if (ok) {
            count_completed_stroke(steno);
        }
        return ok;
    }

    Translation_Match match = {0};
    if (!translation_match_find(
            &steno->dictionary_stack.dictionary,
            steno->translations,
            bits,
            &match)) {
        return false;
    }
    set_replacement_format_case_state(steno, &match);

    char *phrase_translation = NULL;
    bool phrase = false;
    if (match.translation == NULL) {
        const Phrase_Lookup_Result result = phrasing_lookup(
            steno->phrasing,
            bits,
            &phrase_translation
        );
        if (result == PHRASE_LOOKUP_ERROR) {
            free(phrase_translation);
            translation_match_destroy(&match);
            return false;
        }
        if (result == PHRASE_LOOKUP_HIT) {
            match.translation = phrase_translation;
            phrase = true;
        }
    }

    trace_stroke_with_mode(
        steno,
        match.outline,
        match.translation,
        phrase ? TRACE_STROKE_PHRASE : trace_mode
    );
    const bool ok = steno_apply_translation_match(steno, &match);
    if (ok) {
        if (match.translation != NULL && !phrase) {
            steno_maybe_emit_brevity_suggestion(steno);
        }
        count_completed_stroke(steno);
    }
    free(phrase_translation);
    translation_match_destroy(&match);
    return ok;
}

bool steno_translate_chord_bits(Steno *steno, uint64_t bits)
{
    return translate_dictionary_bits_with_trace(steno, bits, TRACE_STROKE_NORMAL);
}

bool steno_translate_stroke_input(Steno *steno, Stroke_Input stroke)
{
    return translate_dictionary_bits_with_trace(steno, stroke.bits, TRACE_STROKE_NORMAL);
}
