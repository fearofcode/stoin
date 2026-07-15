#include "steno_internal.h"

#include "retro.h"
#include "stitch.h"
#include "text_util.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

typedef struct Steno_Case_State {
    Case_Mode case_mode;
    Case_Mode next_case;
} Steno_Case_State;

static const char *steno_spacing(const Steno *steno)
{
    return steno != NULL && steno->spacing.spacing != NULL ? steno->spacing.spacing : "";
}

static Steno_Case_State steno_case_state(const Steno *steno)
{
    Steno_Case_State state = {
        .case_mode = steno != NULL ? steno->case_mode : CASE_MODE_NORMAL,
        .next_case = steno != NULL ? steno->next_case : CASE_MODE_NORMAL,
    };
    return state;
}

static void steno_restore_case_state(Steno *steno, Steno_Case_State state)
{
    if (steno == NULL) {
        return;
    }
    steno->case_mode = state.case_mode;
    steno->next_case = state.next_case;
}

static void translation_set_previous_case_state(Translation *translation, Steno_Case_State state)
{
    if (translation == NULL) {
        return;
    }
    translation->previous_case_mode = state.case_mode;
    translation->previous_next_case = state.next_case;
    translation->has_case_state = true;
}

static void translation_set_resulting_case_state(Translation *translation, Steno_Case_State state)
{
    if (translation == NULL) {
        return;
    }
    translation->resulting_case_mode = state.case_mode;
    translation->resulting_next_case = state.next_case;
    translation->has_case_state = true;
}

static void translation_restore_previous_case_state(Steno *steno, const Translation *translation)
{
    if (steno == NULL || translation == NULL || !translation->has_case_state) {
        return;
    }
    Steno_Case_State state = {
        .case_mode = translation->previous_case_mode,
        .next_case = translation->previous_next_case,
    };
    steno_restore_case_state(steno, state);
}

static bool text_starts_with_spacing(const Steno *steno, const char *text)
{
    const char *spacing = steno_spacing(steno);
    const size_t spacing_length = strlen(spacing);
    return steno != NULL
        && text != NULL
        && steno->spacing.mode == SPACING_MODE_BEFORE_WORD
        && spacing_length > 0
        && strncmp(text, spacing, spacing_length) == 0;
}

static bool text_ends_with_spacing(const Steno *steno, const char *text)
{
    if (text == NULL) {
        return false;
    }

    const char *spacing = steno_spacing(steno);
    const size_t spacing_length = strlen(spacing);
    const size_t length = strlen(text);
    return steno != NULL
        && steno->spacing.mode == SPACING_MODE_BEFORE_WORD
        && spacing_length > 0
        && length >= spacing_length
        && memcmp(text + length - spacing_length, spacing, spacing_length) == 0;
}

static bool should_prepend_spacing(
    const Steno *steno,
    const Translation *previous,
    const char *old_text,
    const Formatted_Text *formatted
)
{
    const char *spacing = steno_spacing(steno);
    if (steno == NULL
        || formatted == NULL
        || formatted->text == NULL
        || formatted->text[0] == '\0'
        || formatted->attach_prev
        || formatted->glue
        || steno->spacing.mode != SPACING_MODE_BEFORE_WORD
        || spacing[0] == '\0'
        || text_starts_with_spacing(steno, formatted->text)) {
        return false;
    }

    if (previous != NULL) {
        if (previous->next_attach || text_ends_with_spacing(steno, previous->utf8)) {
            return false;
        }
        return true;
    }

    return text_starts_with_spacing(steno, old_text);
}

static const Translation *previous_visible_translation(const Translation *translations, size_t before_index)
{
    while (before_index > 0) {
        const Translation *previous = &translations[before_index - 1];
        if (previous->utf8 != NULL && previous->utf8[0] != '\0') {
            return previous;
        }
        --before_index;
    }
    return NULL;
}

static bool append_orthographic_join(
    Steno *steno,
    char **emitted,
    const char *old_text,
    size_t old_text_length,
    const Formatted_Text *formatted
)
{
    size_t word_start = old_text_length;
    while (word_start > 0 && !isspace((unsigned char)old_text[word_start - 1])) {
        --word_start;
    }

    if (!text_append_range(emitted, old_text, word_start)) {
        return false;
    }

    const size_t word_length = old_text_length - word_start;
    if (word_length == 0) {
        return text_append_range(emitted, formatted->text, strlen(formatted->text));
    }

    char *word = copy_range(old_text + word_start, word_length);
    char *joined = NULL;
    if (word == NULL || !orthography_apply(&steno->orthography, word, formatted->ortho_suffix, &joined)) {
        free(word);
        free(joined);
        return false;
    }

    const bool ok = text_append_range(emitted, formatted->text, formatted->ortho_suffix_text_offset)
        && text_append_range(emitted, joined, strlen(joined))
        && text_append_range(
            emitted,
            formatted->text + formatted->ortho_suffix_text_offset + formatted->ortho_suffix_text_length,
            strlen(formatted->text + formatted->ortho_suffix_text_offset + formatted->ortho_suffix_text_length)
        );
    free(word);
    free(joined);
    return ok;
}

static char *build_emitted_text(
    Steno *steno,
    const char *old_text,
    const Formatted_Text *formatted,
    bool prepend_spacing
)
{
    char *emitted = NULL;
    if (prepend_spacing) {
        const char *spacing = steno_spacing(steno);
        if (!text_append_range(&emitted, spacing, strlen(spacing))) {
            arrfree(emitted);
            return NULL;
        }
    }

    const size_t old_text_length = old_text == NULL ? 0 : strlen(old_text);
    if (formatted->ortho_suffix != NULL && old_text != NULL) {
        if (!append_orthographic_join(steno, &emitted, old_text, old_text_length, formatted)) {
            arrfree(emitted);
            return NULL;
        }
    } else {
        if (formatted->attach_prev && old_text != NULL) {
            if (!text_append_range(&emitted, old_text, old_text_length)) {
                arrfree(emitted);
                return NULL;
            }
        }

        if (!text_append_range(&emitted, formatted->text, strlen(formatted->text))) {
            arrfree(emitted);
            return NULL;
        }
    }

    if (emitted == NULL) {
        arrput(emitted, '\0');
    }
    return emitted;
}

static bool replace_output_text(Steno *steno, const char *old_text, const char *new_text)
{
    const size_t prefix = common_utf8_prefix_bytes(old_text, new_text);
    const char *delete_suffix = old_text + prefix;
    const char *insert_suffix = new_text + prefix;

    if (delete_suffix[0] != '\0' && !steno->delete_text(delete_suffix, steno->send_userdata)) {
        return false;
    }
    if (insert_suffix[0] != '\0' && !steno->send_text(insert_suffix, steno->send_userdata)) {
        return false;
    }

    return true;
}

static bool send_key_combinations(Steno *steno, char *const *key_combos)
{
    for (size_t i = 0; i < arrlenu(key_combos); ++i) {
        if (steno->send_key_combination == NULL) {
            fprintf(stderr, "stoin: key combo '%s' is not supported on this platform\n", key_combos[i]);
            return false;
        }
        if (!steno->send_key_combination(key_combos[i], steno->send_userdata)) {
            fprintf(stderr, "stoin: failed to send key combo '%s'\n", key_combos[i]);
            return false;
        }
    }
    return true;
}

static bool undo_last_translation(Steno *steno)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return true;
    }

    Translation translation = steno->translations[translation_count - 1];
    char *replacement_text = translation_replaced_text(&translation);
    if (replacement_text == NULL) {
        return false;
    }

    if (!replace_output_text(steno, translation.utf8, replacement_text)) {
        arrfree(replacement_text);
        return false;
    }

    translation_restore_previous_case_state(steno, &translation);

    arrsetlen(steno->translations, translation_count - 1);
    for (size_t i = 0; i < arrlenu(translation.replaced); ++i) {
        arrput(steno->translations, translation.replaced[i]);
    }

    arrsetlen(translation.replaced, 0);
    translation_destroy(&translation);
    arrfree(replacement_text);
    return true;
}

static bool replace_output_callback(void *userdata, const char *old_text, const char *new_text)
{
    return replace_output_text(userdata, old_text, new_text);
}

static bool retro_undo_last_translation_callback(void *userdata)
{
    return undo_last_translation(userdata);
}

static bool retro_translate_bits_callback(
    void *userdata,
    uint64_t bits
)
{
    return steno_translate_chord_bits(userdata, bits);
}

static Retro_Context make_retro_context(Steno *steno)
{
    Retro_Context context = {
        .translations = &steno->translations,
        .spacing = &steno->spacing,
        .replace_output = replace_output_callback,
        .undo_last_translation = retro_undo_last_translation_callback,
        .translate_bits = retro_translate_bits_callback,
        .userdata = steno,
    };
    return context;
}

static Stitch_Context make_stitch_context(Steno *steno)
{
    Stitch_Context context = {
        .translations = &steno->translations,
        .replace_output = replace_output_callback,
        .userdata = steno,
    };
    return context;
}

static bool repeat_last_translation(
    Steno *steno,
    const uint64_t *strokes,
    size_t stroke_count
)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return true;
    }

    const Translation *last = &steno->translations[translation_count - 1];
    const Steno_Case_State previous_case_state = steno_case_state(steno);
    Translation next = {
        .glue = last->glue,
        .next_attach = last->next_attach,
    };
    const bool repeat_needs_spacing = steno->spacing.mode == SPACING_MODE_BEFORE_WORD
        && !last->glue
        && !last->next_attach
        && last->utf8 != NULL
        && last->utf8[0] != '\0'
        && !text_starts_with_spacing(steno, last->utf8);
    if ((repeat_needs_spacing
            && !text_append_cstring(&next.utf8, steno_spacing(steno)))
        || !text_append_cstring(&next.utf8, last->utf8)
        || !translation_set_strokes(&next, strokes, stroke_count)) {
        translation_destroy(&next);
        return false;
    }

    translation_set_previous_case_state(&next, previous_case_state);
    translation_set_resulting_case_state(&next, steno_case_state(steno));

    if (!replace_output_text(steno, "", next.utf8)) {
        translation_destroy(&next);
        return false;
    }

    arrput(steno->translations, next);
    return true;
}

bool steno_execute_command(
    Steno *steno,
    const char *command,
    const uint64_t *strokes,
    size_t stroke_count
)
{
    if (strcmp(command, "=undo") == 0) {
        return undo_last_translation(steno);
    }
    if (strcmp(command, "=repeat_last_translation") == 0) {
        return repeat_last_translation(steno, strokes, stroke_count);
    }

    fprintf(stderr, "stoin: unknown dictionary command '%s'\n", command);
    return true;
}

static bool execute_mode_command(Steno *steno, const char *command)
{
    if (steno == NULL || command == NULL) {
        return false;
    }

    const char *separator = strchr(command, ':');
    const size_t name_length = separator == NULL ? strlen(command) : (size_t)(separator - command);
    const char *argument = separator == NULL ? NULL : separator + 1;

    if (ascii_range_equals_ignore_case(command, name_length, "set_space")) {
        return steno_set_spacing(steno, argument != NULL ? argument : "");
    }

    if (argument != NULL) {
        fprintf(stderr, "stoin: unsupported mode command '%s'\n", command);
        return true;
    }

    if (ascii_range_equals_ignore_case(command, name_length, "caps")) {
        steno->case_mode = CASE_MODE_UPPER;
        return true;
    }
    if (ascii_range_equals_ignore_case(command, name_length, "title")) {
        steno->case_mode = CASE_MODE_TITLE;
        return true;
    }
    if (ascii_range_equals_ignore_case(command, name_length, "lower")) {
        steno->case_mode = CASE_MODE_LOWER;
        return true;
    }
    if (ascii_range_equals_ignore_case(command, name_length, "snake")) {
        return steno_set_spacing(steno, "_");
    }
    if (ascii_range_equals_ignore_case(command, name_length, "camel")) {
        steno->case_mode = CASE_MODE_TITLE;
        steno->next_case = CASE_MODE_LOWER_FIRST_CHAR;
        return steno_set_spacing(steno, "");
    }
    if (ascii_range_equals_ignore_case(command, name_length, "reset")) {
        steno->case_mode = CASE_MODE_NORMAL;
        steno->next_case = CASE_MODE_NORMAL;
        return steno_set_spacing(steno, " ");
    }
    if (ascii_range_equals_ignore_case(command, name_length, "reset_space")) {
        return steno_set_spacing(steno, " ");
    }
    if (ascii_range_equals_ignore_case(command, name_length, "reset_case")) {
        steno->case_mode = CASE_MODE_NORMAL;
        steno->next_case = CASE_MODE_NORMAL;
        return true;
    }

    fprintf(stderr, "stoin: unsupported mode command '%s'\n", command);
    return true;
}

static bool execute_plover_command(Steno *steno, const char *command)
{
    if (command == NULL || command[0] == '\0') {
        return true;
    }

    if (ascii_range_starts_with_ignore_case(command, strlen(command), "toggle_dict:")) {
        return dictionary_stack_toggle(&steno->dictionary_stack, command + strlen("toggle_dict:"));
    }

    (void)steno;
    return true;
}

static void apply_case_state_to_formatted(Steno *steno, Formatted_Text *formatted)
{
    if (formatted->cancel_formatting) {
        steno->next_case = CASE_MODE_NORMAL;
    }
    if (formatted->carry_case
        && formatted->next_case == CASE_MODE_NORMAL
        && steno->next_case != CASE_MODE_NORMAL) {
        formatted->next_case = steno->next_case;
    }
    if (formatted->text[0] != '\0') {
        formatted_text_apply_case(formatted->text, steno->case_mode);
        const Case_Mode text_case = formatted->text_case != CASE_MODE_NORMAL
            ? formatted->text_case
            : steno->next_case;
        formatted_text_apply_case(formatted->text, text_case);
        steno->next_case = formatted->next_case;
    } else if (formatted->next_case != CASE_MODE_NORMAL) {
        steno->next_case = formatted->next_case;
    }
}

static bool formatted_has_deferred_action(const Formatted_Text *formatted)
{
    return formatted->retro_command != RETRO_COMMAND_NONE
        || formatted->stitch_last_word
        || formatted->mode_command != NULL
        || formatted->plover_command != NULL
        || arrlenu(formatted->key_combos) != 0;
}

static bool text_has_prefix(const char *text, const char *prefix)
{
    if (text == NULL || prefix == NULL) {
        return false;
    }
    const size_t prefix_length = strlen(prefix);
    return strlen(text) >= prefix_length && memcmp(text, prefix, prefix_length) == 0;
}

static bool translation_set_split_prefix(
    Translation *translation,
    const char *text,
    size_t stroke_count
)
{
    if (translation == NULL) {
        return false;
    }

    arrfree(translation->split_prefix_text);
    translation->split_prefix_text = NULL;
    translation->split_prefix_stroke_count = 0;
    if (stroke_count == 0) {
        return true;
    }

    if (!text_append_cstring(&translation->split_prefix_text, text != NULL ? text : "")) {
        return false;
    }
    translation->split_prefix_stroke_count = stroke_count;
    return true;
}

static bool match_has_partial_prefix(const Translation_Match *match)
{
    return match != NULL && match->partial_prefix_stroke_count > 0;
}

static bool translation_preserve_partial_segment_boundaries(
    Translation *translation,
    const Translation_Match *match,
    const Translation *replaced
)
{
    if (!match_has_partial_prefix(match)) {
        return true;
    }

    for (size_t i = 0; replaced != NULL && i < arrlenu(replaced->segment_boundaries); ++i) {
        const Translation_Segment_Boundary *source = &replaced->segment_boundaries[i];
        if (source->stroke_count >= match->partial_prefix_stroke_count) {
            break;
        }
        Translation_Segment_Boundary boundary = {
            .stroke_count = source->stroke_count,
        };
        if (!text_append_cstring(&boundary.utf8, source->utf8)) {
            arrfree(boundary.utf8);
            return false;
        }
        arrput(translation->segment_boundaries, boundary);
    }

    Translation_Segment_Boundary boundary = {
        .stroke_count = match->partial_prefix_stroke_count,
    };
    if (!text_append_cstring(
            &boundary.utf8,
            match->partial_prefix_text != NULL ? match->partial_prefix_text : "")) {
        arrfree(boundary.utf8);
        return false;
    }
    arrput(translation->segment_boundaries, boundary);
    return true;
}

static char *build_partial_replacement_text(
    Steno *steno,
    const char *prefix_text,
    const Translation *previous,
    const Formatted_Text *formatted
)
{
    const char *prefix = prefix_text != NULL ? prefix_text : "";
    if (formatted->attach_prev) {
        return build_emitted_text(
            steno,
            prefix,
            formatted,
            should_prepend_spacing(steno, previous, prefix, formatted)
        );
    }

    const Translation synthetic_previous = {
        .utf8 = (char *)prefix,
    };
    const Translation *previous_for_suffix = prefix[0] != '\0' ? &synthetic_previous : previous;
    char *suffix_text = build_emitted_text(
        steno,
        "",
        formatted,
        should_prepend_spacing(steno, previous_for_suffix, "", formatted)
    );
    if (suffix_text == NULL) {
        return NULL;
    }

    char *result = NULL;
    if (!text_append_cstring(&result, prefix)
        || !text_append_cstring(&result, suffix_text)) {
        arrfree(result);
        result = NULL;
    }
    arrfree(suffix_text);
    return result;
}

static bool apply_suffix_translation_match(Steno *steno, const Translation_Match *match)
{
    if (match->suffix_base_translation == NULL || match->suffix_translation == NULL) {
        return false;
    }

    Formatted_Text base = {0};
    Formatted_Text suffix = {0};
    if (!format_translation_text(match->suffix_base_translation, &base)
        || !format_translation_text(match->suffix_translation, &suffix)
        || formatted_has_deferred_action(&base)
        || formatted_has_deferred_action(&suffix)) {
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    const Steno_Case_State previous_case_state = steno_case_state(steno);
    if (match->has_format_case_state) {
        steno_restore_case_state(steno, ((Steno_Case_State) {
            .case_mode = match->format_case_mode,
            .next_case = match->format_next_case,
        }));
    }
    apply_case_state_to_formatted(steno, &base);
    const Steno_Case_State formatted_case_state = steno_case_state(steno);
    steno_restore_case_state(steno, previous_case_state);

    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    size_t replaced_count = match->replaced_count;
    bool auto_split_prefix = false;
    const char *auto_split_prefix_text = NULL;
    const size_t matched_replace_start = translation_count - match->replaced_count;
    const bool retroactive_attach = base.attach_prev
        && match->replaced_count > 0
        && !match_has_partial_prefix(match);
    if (replaced_count == 0 && translation_count > 0) {
        const Translation *previous = &steno->translations[translation_count - 1];
        if (base.stitch && previous->glue) {
            const char *delimiter = base.stitch_delimiter != NULL ? base.stitch_delimiter : "-";
            if (!text_prepend_range(&base.text, delimiter, strlen(delimiter))) {
                formatted_text_destroy(&base);
                formatted_text_destroy(&suffix);
                return false;
            }
        }
        if (base.attach_prev || (base.glue && previous->glue)) {
            replaced_count = 1;
            base.attach_prev = true;
            auto_split_prefix = true;
            auto_split_prefix_text = previous->utf8;
        }
    } else if (retroactive_attach && matched_replace_start > 0) {
        size_t prefix_start = matched_replace_start;
        while (prefix_start > 0) {
            --prefix_start;
            const Translation *prefix = &steno->translations[prefix_start];
            if (prefix->utf8 != NULL && prefix->utf8[0] != '\0') {
                replaced_count += matched_replace_start - prefix_start;
                auto_split_prefix = true;
                auto_split_prefix_text = prefix->utf8;
                break;
            }
        }
    }

    const size_t replace_start = translation_count - replaced_count;
    char *old_text = translation_range_text(steno->translations, replace_start, replaced_count);
    char *base_text = NULL;
    char *final_text = NULL;
    if (old_text == NULL) {
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    const Translation *previous = previous_visible_translation(steno->translations, replace_start);
    if (match_has_partial_prefix(match)) {
        base_text = build_partial_replacement_text(
            steno,
            match->partial_prefix_text,
            previous,
            &base
        );
    } else {
        const char *format_base_text = retroactive_attach
            ? (auto_split_prefix_text != NULL ? auto_split_prefix_text : "")
            : (auto_split_prefix ? auto_split_prefix_text : old_text);
        base_text = build_emitted_text(
            steno,
            format_base_text,
            &base,
            should_prepend_spacing(steno, previous, format_base_text, &base));
    }
    if (base_text == NULL) {
        arrfree(old_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    final_text = build_emitted_text(steno, base_text, &suffix, false);
    if (final_text == NULL) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    Translation next = {
        .utf8 = final_text,
        .glue = suffix.glue,
        .next_attach = suffix.attach_next,
    };
    translation_set_previous_case_state(&next, previous_case_state);
    final_text = NULL;
    size_t split_prefix_stroke_count = 0;
    if (match_has_partial_prefix(match)) {
        if (replace_start >= translation_count
            || match->partial_prefix_stroke_count > arrlenu(steno->translations[replace_start].strokes)
            || !translation_set_strokes(
                &next,
                steno->translations[replace_start].strokes,
                match->partial_prefix_stroke_count)) {
            arrfree(old_text);
            arrfree(base_text);
            formatted_text_destroy(&base);
            formatted_text_destroy(&suffix);
            translation_destroy(&next);
            return false;
        }
    } else if (auto_split_prefix) {
        for (size_t i = replace_start; i < matched_replace_start; ++i) {
            if (!translation_set_strokes(
                    &next,
                    steno->translations[i].strokes,
                    arrlenu(steno->translations[i].strokes))) {
                arrfree(old_text);
                arrfree(base_text);
                formatted_text_destroy(&base);
                formatted_text_destroy(&suffix);
                translation_destroy(&next);
                return false;
            }
        }
        split_prefix_stroke_count = arrlenu(next.strokes);
    }
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }
    if (!translation_preserve_partial_segment_boundaries(
            &next,
            match,
            match_has_partial_prefix(match) ? &steno->translations[replace_start] : NULL)) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }
    if ((match_has_partial_prefix(match)
            && !translation_set_split_prefix(
                &next,
                match->partial_prefix_text,
                match->partial_prefix_stroke_count))
        || (auto_split_prefix
            && text_has_prefix(next.utf8, auto_split_prefix_text)
            && !translation_set_split_prefix(
                &next,
                auto_split_prefix_text,
                split_prefix_stroke_count))) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }

    steno_restore_case_state(steno, formatted_case_state);
    if (!replace_output_text(steno, old_text, next.utf8)) {
        steno_restore_case_state(steno, previous_case_state);
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }

    translation_set_resulting_case_state(&next, steno_case_state(steno));

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, steno->translations[i]);
    }
    arrsetlen(steno->translations, replace_start);
    arrput(steno->translations, next);

    arrfree(old_text);
    arrfree(base_text);
    formatted_text_destroy(&base);
    formatted_text_destroy(&suffix);
    return true;
}

bool steno_apply_translation_match(Steno *steno, const Translation_Match *match)
{
    if (steno == NULL || match == NULL) {
        return false;
    }

    const char *translation_text = match->translation != NULL ? match->translation : match->outline;
    Formatted_Text formatted = {0};
    if (!format_translation_text(translation_text, &formatted)) {
        formatted_text_destroy(&formatted);
        return false;
    }

    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        formatted_text_destroy(&formatted);
        return false;
    }

    if (match->suffix_match) {
        formatted_text_destroy(&formatted);
        return apply_suffix_translation_match(steno, match);
    }

    if (formatted.retro_command != RETRO_COMMAND_NONE) {
        Retro_Context retro = make_retro_context(steno);
        bool ok = false;
        switch (formatted.retro_command) {
        case RETRO_COMMAND_TOGGLE_ASTERISK:
            ok = retro_apply_toggle_asterisk(&retro);
            break;
        case RETRO_COMMAND_DELETE_SPACE:
            ok = retro_apply_delete_space(&retro, match->strokes, match->stroke_count);
            break;
        case RETRO_COMMAND_INSERT_SPACE:
            ok = retro_apply_insert_space(&retro, match->strokes, match->stroke_count);
            break;
        case RETRO_COMMAND_NONE:
            ok = true;
            break;
        }
        formatted_text_destroy(&formatted);
        return ok;
    }

    if (formatted.stitch_last_word) {
        Stitch_Context stitch = make_stitch_context(steno);
        const bool ok = stitch_apply_retro(
            &stitch,
            match->strokes,
            match->stroke_count,
            match->replaced_count,
            formatted.stitch_count,
            formatted.stitch_delimiter
        );
        formatted_text_destroy(&formatted);
        return ok;
    }

    if (formatted.retro_case != CASE_MODE_NORMAL) {
        Retro_Context retro = make_retro_context(steno);
        const bool ok = retro_apply_case(&retro, match->strokes, match->stroke_count, formatted.retro_case);
        formatted_text_destroy(&formatted);
        return ok;
    }

    if (formatted.mode_command != NULL
        && formatted.text[0] == '\0'
        && arrlenu(formatted.key_combos) == 0
        && !formatted.attach_prev
        && !formatted.attach_next
        && formatted.plover_command == NULL) {
        const bool ok = execute_mode_command(steno, formatted.mode_command);
        formatted_text_destroy(&formatted);
        return ok;
    }

    if (formatted.plover_command != NULL
        && formatted.text[0] == '\0'
        && arrlenu(formatted.key_combos) == 0
        && !formatted.attach_prev
        && !formatted.attach_next) {
        const bool ok = execute_plover_command(steno, formatted.plover_command);
        formatted_text_destroy(&formatted);
        return ok;
    }

    const Steno_Case_State previous_case_state = steno_case_state(steno);
    if (match->has_format_case_state) {
        steno_restore_case_state(steno, ((Steno_Case_State) {
            .case_mode = match->format_case_mode,
            .next_case = match->format_next_case,
        }));
    }
    apply_case_state_to_formatted(steno, &formatted);
    const Steno_Case_State formatted_case_state = steno_case_state(steno);
    steno_restore_case_state(steno, previous_case_state);

    size_t replaced_count = match->replaced_count;
    bool auto_split_prefix = false;
    const char *auto_split_prefix_text = NULL;
    const size_t matched_replace_start = translation_count - match->replaced_count;
    const bool retroactive_attach = formatted.attach_prev
        && match->replaced_count > 0
        && !match_has_partial_prefix(match);
    if (replaced_count == 0 && translation_count > 0) {
        const Translation *previous = &steno->translations[translation_count - 1];
        if (formatted.stitch && previous->glue) {
            if (!text_prepend_range(
                    &formatted.text,
                    formatted.stitch_delimiter != NULL ? formatted.stitch_delimiter : "-",
                    strlen(formatted.stitch_delimiter != NULL ? formatted.stitch_delimiter : "-"))) {
                formatted_text_destroy(&formatted);
                return false;
            }
        }
        if (formatted.attach_prev || (formatted.glue && previous->glue)) {
            replaced_count = 1;
            formatted.attach_prev = true;
            auto_split_prefix = true;
            auto_split_prefix_text = previous->utf8;
        }
    } else if (retroactive_attach && matched_replace_start > 0) {
        size_t prefix_start = matched_replace_start;
        while (prefix_start > 0) {
            --prefix_start;
            const Translation *prefix = &steno->translations[prefix_start];
            if (prefix->utf8 != NULL && prefix->utf8[0] != '\0') {
                replaced_count += matched_replace_start - prefix_start;
                auto_split_prefix = true;
                auto_split_prefix_text = prefix->utf8;
                break;
            }
        }
    }

    const size_t replace_start = translation_count - replaced_count;
    char *old_text = translation_range_text(steno->translations, replace_start, replaced_count);
    if (old_text == NULL) {
        formatted_text_destroy(&formatted);
        return false;
    }

    Translation next = {0};
    const Translation *previous = previous_visible_translation(steno->translations, replace_start);
    if (match_has_partial_prefix(match)) {
        next.utf8 = build_partial_replacement_text(
            steno,
            match->partial_prefix_text,
            previous,
            &formatted
        );
    } else {
        const char *format_base_text = retroactive_attach
            ? (auto_split_prefix_text != NULL ? auto_split_prefix_text : "")
            : (auto_split_prefix ? auto_split_prefix_text : old_text);
        next.utf8 = build_emitted_text(
            steno,
            format_base_text,
            &formatted,
            should_prepend_spacing(
                steno,
                previous,
                format_base_text,
                &formatted));
    }
    next.glue = formatted.glue;
    next.next_attach = formatted.attach_next;
    translation_set_previous_case_state(&next, previous_case_state);

    if (next.utf8 == NULL) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }

    size_t split_prefix_stroke_count = 0;
    if (match_has_partial_prefix(match)) {
        if (replace_start >= translation_count
            || match->partial_prefix_stroke_count > arrlenu(steno->translations[replace_start].strokes)
            || !translation_set_strokes(
                &next,
                steno->translations[replace_start].strokes,
                match->partial_prefix_stroke_count)) {
            arrfree(old_text);
            formatted_text_destroy(&formatted);
            translation_destroy(&next);
            return false;
        }
    } else if (auto_split_prefix) {
        for (size_t i = replace_start; i < matched_replace_start; ++i) {
            if (!translation_set_strokes(
                    &next,
                    steno->translations[i].strokes,
                    arrlenu(steno->translations[i].strokes))) {
                arrfree(old_text);
                formatted_text_destroy(&formatted);
                translation_destroy(&next);
                return false;
            }
        }
        split_prefix_stroke_count = arrlenu(next.strokes);
    }
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }
    if (!translation_preserve_partial_segment_boundaries(
            &next,
            match,
            match_has_partial_prefix(match) ? &steno->translations[replace_start] : NULL)) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }
    if ((match_has_partial_prefix(match)
            && !translation_set_split_prefix(
                &next,
                match->partial_prefix_text,
                match->partial_prefix_stroke_count))
        || (auto_split_prefix
            && text_has_prefix(next.utf8, auto_split_prefix_text)
            && !translation_set_split_prefix(
                &next,
                auto_split_prefix_text,
                split_prefix_stroke_count))) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }

    steno_restore_case_state(steno, formatted_case_state);
    if (!replace_output_text(steno, old_text, next.utf8)
        || !send_key_combinations(steno, formatted.key_combos)
        || (formatted.mode_command != NULL && !execute_mode_command(steno, formatted.mode_command))
        || (formatted.plover_command != NULL && !execute_plover_command(steno, formatted.plover_command))) {
        steno_restore_case_state(steno, previous_case_state);
        arrfree(old_text);
        translation_destroy(&next);
        formatted_text_destroy(&formatted);
        return false;
    }

    translation_set_resulting_case_state(&next, steno_case_state(steno));

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, steno->translations[i]);
    }
    arrsetlen(steno->translations, replace_start);
    arrput(steno->translations, next);

    arrfree(old_text);
    formatted_text_destroy(&formatted);
    return true;
}
