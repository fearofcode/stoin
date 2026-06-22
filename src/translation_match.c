#include "translation_match.h"

#include "steno_stroke.h"
#include "text_util.h"

#include <string.h>

#include "../stb_ds.h"

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

void translation_match_destroy(Translation_Match *match)
{
    translation_match_clear_owned(match);
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

    for (size_t i = 0; i < sizeof(suffix_keys) / sizeof(suffix_keys[0]); ++i) {
        const Steno_Key suffix_key = suffix_keys[i];
        const uint64_t suffix_bit = steno_bit(suffix_key);
        const uint64_t last_stroke = candidate[candidate_count - 1];
        if ((last_stroke & suffix_bit) == 0
            || (last_stroke & bits_after_steno_key(suffix_key)) != 0) {
            continue;
        }

        uint64_t base_candidate[TRANSLATION_MATCH_MAX_STROKES] = {0};
        memcpy(base_candidate, candidate, candidate_count * sizeof(candidate[0]));
        base_candidate[candidate_count - 1] &= ~suffix_bit;
        if (base_candidate[candidate_count - 1] == 0) {
            continue;
        }

        const char *base_translation = dictionary_lookup_strokes(dictionary, base_candidate, candidate_count);
        if (base_translation == NULL || base_translation[0] == '=') {
            continue;
        }

        const char *suffix_translation = dictionary_lookup_bits(dictionary, suffix_bit);
        if (suffix_translation == NULL || suffix_translation[0] == '=') {
            continue;
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

    return true;
}

bool translation_match_find(
    const Dictionary *dictionary,
    const Translation *history,
    uint64_t bits,
    Translation_Match *out_match
)
{
    if (dictionary == NULL || out_match == NULL) {
        return false;
    }

    const size_t max_strokes = translation_match_lookup_stroke_limit(dictionary);

    uint64_t candidate[TRANSLATION_MATCH_MAX_STROKES] = { bits };
    size_t candidate_count = 1;
    size_t replaced_count = 0;
    bool found = false;

    const char *translation = dictionary_lookup_strokes(dictionary, candidate, candidate_count);
    if (translation != NULL && translation[0] != '=') {
        if (!set_translation_match(out_match, translation, candidate, candidate_count, replaced_count)) {
            return false;
        }
        found = true;
    } else if (translation == NULL) {
        if (!try_suffix_translation_match(dictionary, candidate, candidate_count, replaced_count, out_match, &found)) {
            return false;
        }
    }

    for (size_t i = arrlenu(history); i > 0 && candidate_count < max_strokes;) {
        --i;
        const Translation *previous = &history[i];
        const size_t previous_stroke_count = arrlenu(previous->strokes);
        if (previous_stroke_count == 0 || candidate_count + previous_stroke_count > max_strokes) {
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

        translation = dictionary_lookup_strokes(dictionary, candidate, candidate_count);
        if (translation != NULL && translation[0] != '=') {
            if (!set_translation_match(out_match, translation, candidate, candidate_count, replaced_count)) {
                return false;
            }
            found = true;
        } else if (translation == NULL) {
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
            if (suffix_found) {
                found = true;
            }
        }
    }

    if (!found && !set_translation_match(out_match, NULL, &bits, 1, 0)) {
        return false;
    }
    return true;
}
