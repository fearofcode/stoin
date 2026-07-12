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
    TRACE_STROKE_PHASE_FALLBACK,
    TRACE_STROKE_MODAL,
    TRACE_STROKE_MODAL_FALLBACK,
} Trace_Stroke_Mode;

static const char *trace_stroke_mode_label(Trace_Stroke_Mode mode)
{
    switch (mode) {
    case TRACE_STROKE_PHRASE:
        return " [phrase]";
    case TRACE_STROKE_PHASE_FALLBACK:
        return " [phase fallback]";
    case TRACE_STROKE_MODAL:
        return " [modal]";
    case TRACE_STROKE_MODAL_FALLBACK:
        return " [modal fallback]";
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
    size_t limit = translation_match_lookup_stroke_limit(&steno->dictionary_stack.dictionary);
    if (steno->modal_dictionary_loaded) {
        const size_t modal_limit = translation_match_lookup_stroke_limit(
            &steno->modal_dictionary_stack.dictionary
        );
        if (modal_limit > limit) {
            limit = modal_limit;
        }
    }
    return limit;
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

static void close_modal_run(Steno *steno)
{
    if (steno == NULL) {
        return;
    }
    arrsetlen(steno->modal_run_strokes, 0);
    steno->modal_run_translation_count = 0;
    steno->modal_run_open = false;
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

static bool translate_phrase_bits(Steno *steno, uint64_t bits, bool *out_hit)
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
    const Phrase_Lookup_Result result = phrasing_lookup(steno->phrasing, bits, &phrase_text);
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
    match.source = TRANSLATION_SOURCE_PHRASE;
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
    match.source = trace_mode == TRACE_STROKE_PHRASE
        ? TRANSLATION_SOURCE_PHRASE
        : TRANSLATION_SOURCE_NORMAL;
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
        const bool ok = steno_execute_command(
            steno,
            single_stroke_translation,
            &bits,
            1,
            TRANSLATION_SOURCE_NORMAL);
        if (ok) {
            count_completed_stroke(steno);
        }
        return ok;
    }

    Translation_Match match = {0};
    if (!translation_match_find_for_source(
            &steno->dictionary_stack.dictionary,
            steno->translations,
            arrlenu(steno->translations),
            TRANSLATION_SOURCE_NORMAL,
            bits,
            &match)) {
        return false;
    }

    bool modal_fallback = false;
    if (match.translation == NULL && steno->modal_dictionary_loaded) {
        const char *fallback = dictionary_lookup_bits(
            &steno->modal_dictionary_stack.dictionary,
            bits
        );
        if (fallback != NULL) {
            if (fallback[0] == '=') {
                trace_stroke_with_mode(steno, raw_chord, fallback, TRACE_STROKE_MODAL_FALLBACK);
                translation_match_destroy(&match);
                const bool ok = steno_execute_command(
                    steno,
                    fallback,
                    &bits,
                    1,
                    TRANSLATION_SOURCE_NORMAL);
                if (ok) {
                    count_completed_stroke(steno);
                }
                return ok;
            }
            match.translation = fallback;
            modal_fallback = true;
        }
    }

    trace_stroke_with_mode(
        steno,
        match.outline,
        match.translation,
        modal_fallback ? TRACE_STROKE_MODAL_FALLBACK : trace_mode
    );
    const bool ok = steno_apply_translation_match(steno, &match);
    if (ok) {
        if (match.translation != NULL && !modal_fallback) {
            steno_maybe_emit_brevity_suggestion(steno);
        }
        count_completed_stroke(steno);
    }
    translation_match_destroy(&match);
    return ok;
}

static void open_modal_run(Steno *steno)
{
    close_modal_run(steno);
    steno->modal_run_case_mode = steno->case_mode;
    steno->modal_run_next_case = steno->next_case;
    steno->modal_run_open = true;
}

static void set_modal_replay_case_state(Steno *steno, Translation_Match *match)
{
    if (steno == NULL
        || match == NULL
        || match->has_format_case_state
        || match->replaced_count == 0
        || match->partial_prefix_stroke_count != 0) {
        return;
    }

    if (match->replaced_count == steno->modal_run_translation_count
        && match->stroke_count == arrlenu(steno->modal_run_strokes)) {
        match->format_case_mode = steno->modal_run_case_mode;
        match->format_next_case = steno->modal_run_next_case;
        match->has_format_case_state = true;
    }
}

static bool modal_history_tail_matches_run(
    const Steno *steno,
    size_t *out_translation_count
)
{
    const size_t translation_count = arrlenu(steno->translations);
    const size_t run_stroke_count = arrlenu(steno->modal_run_strokes);
    if (out_translation_count == NULL) {
        return false;
    }
    if (run_stroke_count == 0) {
        *out_translation_count = 0;
        return true;
    }

    size_t run_start = translation_count;
    size_t tail_stroke_count = 0;
    while (run_start > 0 && tail_stroke_count < run_stroke_count) {
        --run_start;
        const Translation *translation = &steno->translations[run_start];
        if (translation->source != TRANSLATION_SOURCE_MODAL) {
            return false;
        }
        tail_stroke_count += arrlenu(translation->strokes);
    }
    if (tail_stroke_count != run_stroke_count) {
        return false;
    }

    size_t stroke_index = 0;
    for (size_t i = run_start; i < translation_count; ++i) {
        const Translation *translation = &steno->translations[i];
        for (size_t j = 0; j < arrlenu(translation->strokes); ++j) {
            if (stroke_index >= run_stroke_count
                || translation->strokes[j] != steno->modal_run_strokes[stroke_index]) {
                return false;
            }
            ++stroke_index;
        }
    }
    if (stroke_index != run_stroke_count) {
        return false;
    }

    *out_translation_count = translation_count - run_start;
    return true;
}

void steno_prepare_modal_retro_retranslation(Steno *steno)
{
    if (steno == NULL) {
        return;
    }

    const size_t run_stroke_count = arrlenu(steno->modal_run_strokes);
    if (run_stroke_count >= 2) {
        arrsetlen(steno->modal_run_strokes, run_stroke_count - 2);
        size_t translation_count = 0;
        if (modal_history_tail_matches_run(steno, &translation_count)) {
            steno->modal_run_translation_count = translation_count;
            steno->modal_run_open = true;
            return;
        }
    }

    arrsetlen(steno->modal_run_strokes, 0);
    steno->modal_run_translation_count = 0;
    steno->modal_run_case_mode = steno->case_mode;
    steno->modal_run_next_case = steno->next_case;
    steno->modal_run_open = true;
}

static bool translate_modal_dictionary_bits(Steno *steno, uint64_t bits)
{
    if (bits == 0) {
        return true;
    }

    if (!steno->modal_run_open
        || arrlenu(steno->modal_run_strokes) >= TRANSLATION_MATCH_MAX_STROKES
        || steno->modal_run_translation_count > arrlenu(steno->translations)) {
        open_modal_run(steno);
    }
    arrput(steno->modal_run_strokes, bits);

    const Dictionary *dictionary = &steno->modal_dictionary_stack.dictionary;
    const size_t old_modal_count = steno->modal_run_translation_count;
    Translation_Match match = {0};
    bool preferred_found = false;
    if (!translation_match_find_phrase_preferred(
            dictionary,
            steno->modal_run_strokes,
            arrlenu(steno->modal_run_strokes),
            old_modal_count,
            &match,
            &preferred_found)) {
        close_modal_run(steno);
        return false;
    }

    if (preferred_found) {
        match.has_format_case_state = true;
        match.format_case_mode = steno->modal_run_case_mode;
        match.format_next_case = steno->modal_run_next_case;
    } else {
        const char *single = dictionary_lookup_bits(dictionary, bits);
        if (single != NULL && single[0] == '=') {
            char raw_chord[64] = {0};
            if (!chord_bits_to_string(bits, raw_chord, sizeof(raw_chord))) {
                close_modal_run(steno);
                return false;
            }
            trace_stroke_with_mode(steno, raw_chord, single, TRACE_STROKE_MODAL);
            const bool ok = steno_execute_command(
                steno,
                single,
                &bits,
                1,
                TRANSLATION_SOURCE_MODAL);
            close_modal_run(steno);
            if (ok && !steno->modal_retranslation_in_progress) {
                count_completed_stroke(steno);
            }
            return ok;
        }

        const size_t translation_count = arrlenu(steno->translations);
        const Translation *history = old_modal_count > 0
            ? steno->translations + translation_count - old_modal_count
            : NULL;
        if (!translation_match_find_for_source(
                dictionary,
                history,
                old_modal_count,
                TRANSLATION_SOURCE_MODAL,
                bits,
                &match)) {
            close_modal_run(steno);
            return false;
        }
        set_modal_replay_case_state(steno, &match);
    }

    trace_stroke_with_mode(steno, match.outline, match.translation, TRACE_STROKE_MODAL);
    const bool ok = steno_apply_translation_match(steno, &match);
    size_t modal_translation_count = 0;
    if (ok
        && modal_history_tail_matches_run(
            steno,
            &modal_translation_count)) {
        steno->modal_run_translation_count = modal_translation_count;
        if (!steno->modal_retranslation_in_progress) {
            count_completed_stroke(steno);
        }
    } else {
        close_modal_run(steno);
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

static bool translate_phrase_namespace_bits(Steno *steno, uint64_t bits)
{
    if (bits == 0) {
        return true;
    }

    bool phrase_hit = false;
    if (!translate_phrase_bits(steno, bits, &phrase_hit)) {
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
    close_modal_run(steno);
    return translate_dictionary_bits_with_trace(steno, bits, TRACE_STROKE_NORMAL);
}

bool steno_translate_stroke_input(Steno *steno, Stroke_Input stroke)
{
    if (stroke.modal_dictionary) {
        return translate_modal_dictionary_bits(steno, stroke.bits);
    }

    close_modal_run(steno);
    const bool phrase_namespace = stroke.phrase_namespace || steno->phrase_namespace_enabled;
    if (!phrase_namespace) {
        return steno_translate_chord_bits(steno, stroke.bits);
    }

    const Steno_Phrase_Mode phrase_mode = normalize_stroke_phrase_mode(stroke, steno->phrase_mode);
    if (phrase_mode != STENO_PHRASE_MODE_NONE) {
        return translate_phrase_namespace_bits(steno, stroke.bits);
    }
    return translate_dictionary_bits_with_trace(steno, stroke.bits, TRACE_STROKE_NORMAL);
}
