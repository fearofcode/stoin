#include "translation_match.h"

#include "steno_stroke.h"
#include "text_util.h"

#include <string.h>

#include "../third_party/stb_ds.h"

static bool stroke_sequence_to_string(const uint64_t *strokes, size_t stroke_count, char *out, size_t out_size)
{
    if (strokes == NULL || stroke_count == 0 || out == NULL || out_size == 0) {
        return false;
    }

    size_t index = 0;
    out[0] = '\0';

    for (size_t i = 0; i < stroke_count; ++i) {
        char stroke[64] = {0};
        if (!chord_bits_to_string(strokes[i], stroke, sizeof(stroke))) {
            return false;
        }

        const size_t separator_length = i == 0 ? 0 : 1;
        const size_t stroke_length = strlen(stroke);
        if (index + separator_length + stroke_length >= out_size) {
            return false;
        }
        if (i != 0) {
            out[index++] = '/';
        }
        memcpy(out + index, stroke, stroke_length + 1);
        index += stroke_length;
    }

    return true;
}

static void translation_match_clear_owned(Translation_Match *match)
{
    if (match == NULL) {
        return;
    }
    arrfree(match->owned_translation);
    match->owned_translation = NULL;
}

static void translation_match_clear_partial_prefix(Translation_Match *match)
{
    if (match == NULL) {
        return;
    }
    arrfree(match->partial_prefix_text);
    match->partial_prefix_text = NULL;
    match->partial_prefix_stroke_count = 0;
}

static bool translation_match_set_partial_prefix(
    Translation_Match *match,
    const char *text,
    size_t stroke_count
)
{
    if (match == NULL) {
        return false;
    }

    translation_match_clear_partial_prefix(match);
    if (stroke_count == 0) {
        return true;
    }

    if (!text_append_cstring(&match->partial_prefix_text, text != NULL ? text : "")) {
        translation_match_clear_partial_prefix(match);
        return false;
    }
    match->partial_prefix_stroke_count = stroke_count;
    return true;
}

void translation_match_destroy(Translation_Match *match)
{
    translation_match_clear_owned(match);
    translation_match_clear_partial_prefix(match);
}

static bool set_translation_match(
    Translation_Match *match,
    const char *translation,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count
)
{
    translation_match_clear_owned(match);
    translation_match_clear_partial_prefix(match);
    match->translation = translation;
    match->suffix_base_translation = NULL;
    match->suffix_translation = NULL;
    match->suffix_match = false;
    match->stroke_count = stroke_count;
    match->replaced_count = replaced_count;
    memcpy(match->strokes, strokes, stroke_count * sizeof(strokes[0]));
    return stroke_sequence_to_string(strokes, stroke_count, match->outline, sizeof(match->outline));
}

static bool set_suffix_translation_match(
    Translation_Match *match,
    const char *base_translation,
    const char *suffix_translation,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count
)
{
    char *translation = NULL;
    if (!text_append_cstring(&translation, base_translation)
        || !text_append_cstring(&translation, suffix_translation)) {
        arrfree(translation);
        return false;
    }

    if (!set_translation_match(match, translation, strokes, stroke_count, replaced_count)) {
        arrfree(translation);
        return false;
    }

    match->owned_translation = translation;
    match->suffix_base_translation = base_translation;
    match->suffix_translation = suffix_translation;
    match->suffix_match = true;
    return true;
}

size_t translation_match_lookup_stroke_limit(const Dictionary *dictionary)
{
    size_t max_strokes = dictionary_longest_key(dictionary);
    if (max_strokes == 0 || max_strokes > TRANSLATION_MATCH_MAX_STROKES) {
        max_strokes = TRANSLATION_MATCH_MAX_STROKES;
    }
    return max_strokes;
}

static uint64_t bits_after_steno_key(Steno_Key key)
{
    uint64_t bits = 0;
    for (Steno_Key candidate = key + 1; candidate < STENO_KEY_COUNT; ++candidate) {
        bits |= steno_bit(candidate);
    }
    return bits;
}

static bool try_suffix_bits_translation_match(
    const Dictionary *dictionary,
    const uint64_t *candidate,
    size_t candidate_count,
    size_t replaced_count,
    uint64_t suffix_bits,
    uint64_t translation_bits,
    Translation_Match *out_match,
    bool *out_found
)
{
    *out_found = false;

    uint64_t base_candidate[TRANSLATION_MATCH_MAX_STROKES] = {0};
    memcpy(base_candidate, candidate, candidate_count * sizeof(candidate[0]));
    base_candidate[candidate_count - 1] &= ~suffix_bits;
    if (base_candidate[candidate_count - 1] == 0) {
        return true;
    }

    const char *base_translation = dictionary_lookup_strokes(
        dictionary,
        base_candidate,
        candidate_count
    );
    if (base_translation == NULL || base_translation[0] == '=') {
        return true;
    }

    const char *suffix_translation = dictionary_lookup_bits(dictionary, translation_bits);
    if (suffix_translation == NULL || suffix_translation[0] == '=') {
        return true;
    }

    if (!set_suffix_translation_match(
            out_match,
            base_translation,
            suffix_translation,
            candidate,
            candidate_count,
            replaced_count)) {
        return false;
    }
    *out_found = true;
    return true;
}

static bool dictionary_matches_with_only_one_dz_bit_removed(
    const Dictionary *dictionary,
    const uint64_t *candidate,
    size_t candidate_count,
    uint64_t d_bit,
    uint64_t z_bit
)
{
    const uint64_t bits_to_remove[] = { d_bit, z_bit };
    for (size_t i = 0; i < sizeof(bits_to_remove) / sizeof(bits_to_remove[0]); ++i) {
        uint64_t possible_prefix[TRANSLATION_MATCH_MAX_STROKES] = {0};
        memcpy(possible_prefix, candidate, candidate_count * sizeof(candidate[0]));
        possible_prefix[candidate_count - 1] &= ~bits_to_remove[i];
        if (dictionary_lookup_strokes(dictionary, possible_prefix, candidate_count) != NULL) {
            return true;
        }
    }
    return false;
}

static bool try_suffix_translation_match(
    const Dictionary *dictionary,
    const uint64_t *candidate,
    size_t candidate_count,
    size_t replaced_count,
    Translation_Match *out_match,
    bool *out_found
)
{
    static const Steno_Key suffix_keys[] = {
        // Plover's English stenotype SUFFIX_KEYS convention.
        STENO_RIGHT_Z,
        STENO_RIGHT_D,
        STENO_RIGHT_S,
        STENO_RIGHT_G,
    };

    *out_found = false;
    if (candidate_count == 0) {
        return true;
    }

    const uint64_t last_stroke = candidate[candidate_count - 1];
    if (dictionary_lookup_bits(dictionary, last_stroke) != NULL) {
        return true;
    }

    for (size_t i = 0; i < sizeof(suffix_keys) / sizeof(suffix_keys[0]); ++i) {
        const Steno_Key suffix_key = suffix_keys[i];
        const uint64_t suffix_bit = steno_bit(suffix_key);
        if ((last_stroke & suffix_bit) == 0
            || (last_stroke & bits_after_steno_key(suffix_key)) != 0) {
            continue;
        }

        bool suffix_found = false;
        if (!try_suffix_bits_translation_match(
                dictionary,
                candidate,
                candidate_count,
                replaced_count,
                suffix_bit,
                suffix_bit,
                out_match,
                &suffix_found)) {
            return false;
        }
        if (suffix_found) {
            *out_found = true;
            return true;
        }
    }

    const uint64_t d_bit = steno_bit(STENO_RIGHT_D);
    const uint64_t z_bit = steno_bit(STENO_RIGHT_Z);
    const uint64_t dz_bits = d_bit | z_bit;
    if ((last_stroke & dz_bits) != dz_bits) {
        return true;
    }

    // DZ is an alternate -ing chord only when both keys are newly available
    // on the matching prefix stroke. Prefer a dictionary prefix that retains
    // either D or Z rather than stealing one of its keys for this suffix.
    if (dictionary_matches_with_only_one_dz_bit_removed(
            dictionary,
            candidate,
            candidate_count,
            d_bit,
            z_bit)) {
        return true;
    }

    return try_suffix_bits_translation_match(
        dictionary,
        candidate,
        candidate_count,
        replaced_count,
        dz_bits,
        steno_bit(STENO_RIGHT_G),
        out_match,
        out_found
    );
}

static bool try_candidate_translation_match(
    const Dictionary *dictionary,
    const uint64_t *candidate,
    size_t candidate_count,
    size_t replaced_count,
    const char *partial_prefix_text,
    size_t partial_prefix_stroke_count,
    Translation_Match *out_match,
    bool *out_found
)
{
    *out_found = false;

    const char *translation = dictionary_lookup_strokes(dictionary, candidate, candidate_count);
    if (translation != NULL) {
        if (translation[0] == '=') {
            return true;
        }
        if (!set_translation_match(out_match, translation, candidate, candidate_count, replaced_count)
            || !translation_match_set_partial_prefix(
                out_match,
                partial_prefix_text,
                partial_prefix_stroke_count)) {
            return false;
        }
        *out_found = true;
        return true;
    }

    bool suffix_found = false;
    if (!try_suffix_translation_match(
            dictionary,
            candidate,
            candidate_count,
            replaced_count,
            out_match,
            &suffix_found)) {
        return false;
    }
    if (suffix_found
        && !translation_match_set_partial_prefix(
            out_match,
            partial_prefix_text,
            partial_prefix_stroke_count)) {
        return false;
    }
    *out_found = suffix_found;
    return true;
}

static bool translation_has_split_prefix(const Translation *translation)
{
    if (translation == NULL || translation->utf8 == NULL) {
        return false;
    }

    const size_t stroke_count = arrlenu(translation->strokes);
    if (translation->split_prefix_stroke_count == 0
        || translation->split_prefix_stroke_count >= stroke_count) {
        return false;
    }

    const char *prefix = translation->split_prefix_text != NULL
        ? translation->split_prefix_text
        : "";
    return strncmp(translation->utf8, prefix, strlen(prefix)) == 0;
}

bool translation_match_find(
    const Dictionary *dictionary,
    const Translation *history,
    uint64_t bits,
    Translation_Match *out_match
)
{
    const size_t history_count = arrlenu(history);
    if (dictionary == NULL
        || out_match == NULL
        || (history_count > 0 && history == NULL)) {
        return false;
    }

    const size_t max_strokes = translation_match_lookup_stroke_limit(dictionary);

    uint64_t candidate[TRANSLATION_MATCH_MAX_STROKES] = { bits };
    size_t candidate_count = 1;
    size_t replaced_count = 0;
    bool found = false;

    bool candidate_found = false;
    if (!try_candidate_translation_match(
            dictionary,
            candidate,
            candidate_count,
            replaced_count,
            NULL,
            0,
            out_match,
            &candidate_found)) {
        return false;
    }
    found = candidate_found;

    for (size_t i = history_count; i > 0 && candidate_count < max_strokes;) {
        --i;
        const Translation *previous = &history[i];
        const size_t previous_stroke_count = arrlenu(previous->strokes);
        if (previous_stroke_count == 0) {
            break;
        }

        if (translation_has_split_prefix(previous)) {
            const size_t prefix_stroke_count = previous->split_prefix_stroke_count;
            const size_t suffix_stroke_count = previous_stroke_count - prefix_stroke_count;
            if (suffix_stroke_count + candidate_count <= max_strokes) {
                uint64_t partial_candidate[TRANSLATION_MATCH_MAX_STROKES] = {0};
                memcpy(
                    partial_candidate,
                    previous->strokes + prefix_stroke_count,
                    suffix_stroke_count * sizeof(partial_candidate[0])
                );
                memcpy(
                    partial_candidate + suffix_stroke_count,
                    candidate,
                    candidate_count * sizeof(partial_candidate[0])
                );

                bool partial_found = false;
                if (!try_candidate_translation_match(
                        dictionary,
                        partial_candidate,
                        suffix_stroke_count + candidate_count,
                        replaced_count + 1,
                        previous->split_prefix_text,
                        prefix_stroke_count,
                        out_match,
                        &partial_found)) {
                    return false;
                }
                if (partial_found) {
                    found = true;
                }
            }
        }

        for (size_t boundary_index = arrlenu(previous->segment_boundaries);
             boundary_index > 0;
             --boundary_index) {
            const Translation_Segment_Boundary *boundary =
                &previous->segment_boundaries[boundary_index - 1];
            if (boundary->stroke_count == 0
                || boundary->stroke_count >= previous_stroke_count) {
                continue;
            }

            const size_t suffix_stroke_count = previous_stroke_count - boundary->stroke_count;
            if (suffix_stroke_count + candidate_count > max_strokes) {
                continue;
            }

            uint64_t partial_candidate[TRANSLATION_MATCH_MAX_STROKES] = {0};
            memcpy(
                partial_candidate,
                previous->strokes + boundary->stroke_count,
                suffix_stroke_count * sizeof(partial_candidate[0])
            );
            memcpy(
                partial_candidate + suffix_stroke_count,
                candidate,
                candidate_count * sizeof(partial_candidate[0])
            );

            bool partial_found = false;
            if (!try_candidate_translation_match(
                    dictionary,
                    partial_candidate,
                    suffix_stroke_count + candidate_count,
                    replaced_count + 1,
                    boundary->utf8,
                    boundary->stroke_count,
                    out_match,
                    &partial_found)) {
                return false;
            }
            if (partial_found) {
                found = true;
            }
        }

        if (candidate_count + previous_stroke_count > max_strokes) {
            break;
        }

        memmove(
            candidate + previous_stroke_count,
            candidate,
            candidate_count * sizeof(candidate[0])
        );
        memcpy(candidate, previous->strokes, previous_stroke_count * sizeof(candidate[0]));
        candidate_count += previous_stroke_count;
        ++replaced_count;

        candidate_found = false;
        if (!try_candidate_translation_match(
                dictionary,
                candidate,
                candidate_count,
                replaced_count,
                NULL,
                0,
                out_match,
                &candidate_found)) {
            return false;
        }
        if (candidate_found) {
            found = true;
        }
    }

    if (!found && !set_translation_match(out_match, NULL, &bits, 1, 0)) {
        return false;
    }
    return true;
}
