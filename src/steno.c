#include "steno.h"

#include "dictionary_stack.h"
#include "format.h"
#include "keymap.h"
#include "orthography.h"
#include "phrasing.h"
#include "retro.h"
#include "steno_stroke.h"
#include "stitch.h"
#include "text_util.h"
#include "translation_history.h"
#include "translation_match.h"
#include "util.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

enum {
    TRANSLATION_COMPACT_INTERVAL_STROKES = 1000,
    TRANSLATION_HISTORY_STROKE_LIMIT = 1000,
};

struct Steno {
    Keymap keymap;
    Translation *translations;
    Dictionary_Stack dictionary_stack;
    Orthography orthography;
    Phrasing *phrasing;
    char *phrasing_path;
    char *lookup_translation;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    Platform_File_Stamp phrasing_stamp;
    size_t strokes_since_compaction;
    Steno_Phrase_Mode chord_phrase_mode;
    bool phrase_namespace_enabled;
    Steno_Phrase_Mode phrase_mode;
    bool enabled;
    bool session_active;
    bool toggle_esc_down;
    bool control_down;
    bool option_down;
    bool command_down;
    Case_Mode case_mode;
    Case_Mode next_case;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    Send_Key_Combination_Fn send_key_combination;
    void *send_userdata;
    FILE *trace_file;
    bool phrasing_stamp_valid;
    bool phrasing_reload_error_reported;
};

typedef struct Steno_Case_State {
    Case_Mode case_mode;
    Case_Mode next_case;
} Steno_Case_State;

typedef enum Trace_Stroke_Mode {
    TRACE_STROKE_NORMAL,
    TRACE_STROKE_PHRASE,
    TRACE_STROKE_PHASE_FALLBACK,
} Trace_Stroke_Mode;

static bool translate_chord_bits(Steno *steno, uint64_t bits);
static bool translate_chord_bits_with_trace(Steno *steno, uint64_t bits, Trace_Stroke_Mode trace_mode);
static bool translate_stroke_input(Steno *steno, Stroke_Input stroke);

static bool steno_set_spacing(Steno *steno, const char *spacing)
{
    if (steno == NULL) {
        return false;
    }

    char *copy = copy_cstring(spacing != NULL ? spacing : "");
    if (copy == NULL) {
        return false;
    }

    free(steno->spacing.spacing);
    steno->spacing.spacing = copy;
    steno->spacing.mode = SPACING_MODE_BEFORE_WORD;
    return true;
}

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

static void reset_chord(Steno *steno)
{
    steno->down_keycodes = 0;
    steno->chord_bits = 0;
    steno->chord_phrase_mode = STENO_PHRASE_MODE_NONE;
}

enum {
    KEYCODE_ESCAPE = 53,
    KEYCODE_LEFT_COMMAND = 55,
    KEYCODE_RIGHT_COMMAND = 54,
    KEYCODE_LEFT_OPTION = 58,
    KEYCODE_RIGHT_OPTION = 61,
    KEYCODE_LEFT_CONTROL = 59,
    KEYCODE_RIGHT_CONTROL = 62,
    KEYCODE_LEFT_SHIFT = 56,
    KEYCODE_RIGHT_SHIFT = 60,
};

static uint64_t keycode_physical_bit(uint16_t keycode)
{
    return keycode < 64 ? UINT64_C(1) << keycode : 0;
}

static bool update_shortcut_modifier_state(Steno *steno, const Input_Event *event)
{
    switch (event->keycode) {
    case KEYCODE_LEFT_COMMAND:
    case KEYCODE_RIGHT_COMMAND:
        steno->command_down = event->is_down;
        return true;
    case KEYCODE_LEFT_OPTION:
    case KEYCODE_RIGHT_OPTION:
        steno->option_down = event->is_down;
        return true;
    case KEYCODE_LEFT_CONTROL:
    case KEYCODE_RIGHT_CONTROL:
        steno->control_down = event->is_down;
        return true;
    default:
        return false;
    }
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

static bool retro_translate_bits_callback(void *userdata, uint64_t bits)
{
    return translate_chord_bits(userdata, bits);
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

static bool repeat_last_translation(Steno *steno, const uint64_t *strokes, size_t stroke_count)
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

static bool execute_command(Steno *steno, const char *command, const uint64_t *strokes, size_t stroke_count)
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
    apply_case_state_to_formatted(steno, &base);

    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        return false;
    }

    size_t replaced_count = match->replaced_count;
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
    base_text = build_emitted_text(
        steno,
        old_text,
        &base,
        should_prepend_spacing(steno, previous, old_text, &base));
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
    translation_set_resulting_case_state(&next, steno_case_state(steno));
    final_text = NULL;
    if (replaced_count != match->replaced_count) {
        for (size_t i = replace_start; i < translation_count; ++i) {
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
    }
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)) {
        arrfree(old_text);
        arrfree(base_text);
        formatted_text_destroy(&base);
        formatted_text_destroy(&suffix);
        translation_destroy(&next);
        return false;
    }

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

static bool apply_translation_match(Steno *steno, const Translation_Match *match)
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
    apply_case_state_to_formatted(steno, &formatted);

    size_t replaced_count = match->replaced_count;
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
    next.utf8 = build_emitted_text(
        steno,
        old_text,
        &formatted,
        should_prepend_spacing(steno, previous, old_text, &formatted));
    next.glue = formatted.glue;
    next.next_attach = formatted.attach_next;
    translation_set_previous_case_state(&next, previous_case_state);

    if (next.utf8 == NULL) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }

    if (replaced_count != match->replaced_count) {
        for (size_t i = replace_start; i < translation_count; ++i) {
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
    }
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        formatted_text_destroy(&formatted);
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)
        || !send_key_combinations(steno, formatted.key_combos)
        || (formatted.mode_command != NULL && !execute_mode_command(steno, formatted.mode_command))
        || (formatted.plover_command != NULL && !execute_plover_command(steno, formatted.plover_command))) {
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

bool steno_reload_dictionary(Steno *steno)
{
    return steno != NULL && dictionary_stack_reload(&steno->dictionary_stack);
}

bool steno_reload_dictionary_if_changed(Steno *steno)
{
    return steno != NULL && dictionary_stack_reload_if_changed(&steno->dictionary_stack);
}

static bool refresh_phrasing_stamp(Steno *steno)
{
    if (steno == NULL || steno->phrasing_path == NULL) {
        return true;
    }

    Platform_File_Stamp stamp = {0};
    if (!platform_file_stamp(steno->phrasing_path, &stamp)) {
        steno->phrasing_stamp_valid = false;
        return false;
    }

    steno->phrasing_stamp = stamp;
    steno->phrasing_stamp_valid = true;
    return true;
}

static bool phrasing_file_changed(Steno *steno, bool *out_changed)
{
    if (steno == NULL || out_changed == NULL) {
        return false;
    }

    *out_changed = false;
    if (steno->phrasing_path == NULL) {
        return true;
    }
    if (!steno->phrasing_stamp_valid) {
        *out_changed = true;
        return true;
    }

    Platform_File_Stamp stamp = {0};
    if (!platform_file_stamp(steno->phrasing_path, &stamp)) {
        steno->phrasing_stamp_valid = false;
        return false;
    }
    *out_changed = stamp.exists != steno->phrasing_stamp.exists
        || stamp.size != steno->phrasing_stamp.size
        || stamp.modified_time_ns != steno->phrasing_stamp.modified_time_ns;
    return true;
}

bool steno_reload_phrasing(Steno *steno)
{
    if (steno == NULL || steno->phrasing_path == NULL) {
        return true;
    }

    Phrasing *next = phrasing_load(steno->phrasing_path);
    if (next == NULL) {
        (void)refresh_phrasing_stamp(steno);
        if (!steno->phrasing_reload_error_reported) {
            fputs("stoin: phrasing changed but reload failed; keeping previous phrasing\n", stderr);
            steno->phrasing_reload_error_reported = true;
        }
        return false;
    }

    phrasing_destroy(steno->phrasing);
    steno->phrasing = next;
    if (!refresh_phrasing_stamp(steno)) {
        fputs("stoin: warning: reloaded phrasing, but failed to refresh phrasing file stamp\n", stderr);
    }

    steno->phrasing_reload_error_reported = false;
    fprintf(stderr, "stoin: reloaded phrasing from %s\n", steno->phrasing_path);
    return true;
}

bool steno_reload_phrasing_if_changed(Steno *steno)
{
    if (steno == NULL) {
        return false;
    }

    bool changed = false;
    if (!phrasing_file_changed(steno, &changed)) {
        if (!steno->phrasing_reload_error_reported) {
            fputs("stoin: failed to check phrasing file for changes\n", stderr);
            steno->phrasing_reload_error_reported = true;
        }
        return false;
    }

    if (!changed) {
        return true;
    }
    steno->phrasing_reload_error_reported = false;
    return steno_reload_phrasing(steno);
}

bool steno_get_dictionary_paths(const Steno *steno, const char *const **out_paths, size_t *out_path_count)
{
    return dictionary_stack_get_paths(
        steno == NULL ? NULL : &steno->dictionary_stack,
        out_paths,
        out_path_count
    );
}

bool steno_get_phrasing_path(const Steno *steno, const char **out_path)
{
    if (steno == NULL || out_path == NULL) {
        return false;
    }
    *out_path = steno->phrasing_path;
    return true;
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
    const bool ok = apply_translation_match(steno, &match);
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
    const bool ok = apply_translation_match(steno, &match);
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
        const bool ok = execute_command(steno, single_stroke_translation, &bits, 1);
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
    const bool ok = apply_translation_match(steno, &match);
    translation_match_destroy(&match);
    if (ok) {
        count_completed_stroke(steno);
    }
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

static bool translate_chord_bits(Steno *steno, uint64_t bits)
{
    return translate_chord_bits_with_trace(steno, bits, TRACE_STROKE_NORMAL);
}

static bool translate_chord_bits_with_trace(Steno *steno, uint64_t bits, Trace_Stroke_Mode trace_mode)
{
    if (bits == 0) {
        return true;
    }

    bool phrase_hit = false;
    if (!translate_phrase_bits(steno, bits, STENO_PHRASE_MODE_ALL, &phrase_hit)) {
        return false;
    }
    if (phrase_hit) {
        return true;
    }

    return translate_dictionary_bits_with_trace(steno, bits, trace_mode);
}

static bool translate_stroke_input(Steno *steno, Stroke_Input stroke)
{
    const bool phrase_namespace = stroke.phrase_namespace || steno->phrase_namespace_enabled;
    if (!phrase_namespace) {
        return translate_chord_bits(steno, stroke.bits);
    }

    const Steno_Phrase_Mode phrase_mode = normalize_stroke_phrase_mode(stroke, steno->phrase_mode);
    if (phrase_mode != STENO_PHRASE_MODE_NONE) {
        return translate_phrase_namespace_bits(steno, stroke.bits, phrase_mode);
    }
    return translate_dictionary_bits_with_trace(steno, stroke.bits, TRACE_STROKE_NORMAL);
}

static bool translate_completed_stroke_input(Steno *steno, Stroke_Input stroke)
{
    return translate_stroke_input(steno, stroke);
}

Steno *steno_create(const Steno_Config *config)
{
    if (config == NULL || config->send_text == NULL || config->delete_text == NULL) {
        return NULL;
    }

    Steno *steno = calloc(1, sizeof(*steno));
    if (steno == NULL) {
        return NULL;
    }

    steno->enabled = true;
    steno->session_active = true;
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_key_combination = config->send_key_combination;
    steno->send_userdata = config->send_userdata;
    steno->trace_file = config->trace_file;

    if (!steno_set_spacing(steno, " ")
        || (config->keymap_path != NULL && !keymap_load(&steno->keymap, config->keymap_path))) {
        steno_destroy(steno);
        return NULL;
    }

    if (config->phrasing_path != NULL) {
        steno->phrasing_path = copy_cstring(config->phrasing_path);
        if (steno->phrasing_path == NULL) {
            steno_destroy(steno);
            return NULL;
        }
        steno->phrasing = phrasing_load(steno->phrasing_path);
        if (steno->phrasing == NULL) {
            steno_destroy(steno);
            return NULL;
        }
        if (!refresh_phrasing_stamp(steno)) {
            fputs("stoin: warning: failed to capture phrasing file stamp; hot reload may not work\n", stderr);
        }
    }

    if (!dictionary_stack_set_paths(
            &steno->dictionary_stack,
            config->dictionary_path,
            config->dictionary_paths,
            config->dictionary_enabled,
            config->dictionary_path_count)
        || !dictionary_stack_load(&steno->dictionary_stack)) {
        steno_destroy(steno);
        return NULL;
    }

    if (config->word_list_path != NULL && !orthography_load(&steno->orthography, config->word_list_path)) {
        steno_destroy(steno);
        return NULL;
    }

    return steno;
}

void steno_destroy(Steno *steno)
{
    if (steno == NULL) {
        return;
    }

    keymap_destroy(&steno->keymap);
    for (size_t i = 0; i < arrlenu(steno->translations); ++i) {
        translation_destroy(&steno->translations[i]);
    }
    arrfree(steno->translations);
    orthography_destroy(&steno->orthography);
    phrasing_destroy(steno->phrasing);
    dictionary_stack_destroy(&steno->dictionary_stack);
    free(steno->phrasing_path);
    free(steno->lookup_translation);
    free(steno->spacing.spacing);
    free(steno);
}

bool steno_handle_event(Steno *steno, const Input_Event *event)
{
    if (steno == NULL || event == NULL) {
        return false;
    }

    if (!steno->session_active) {
        return false;
    }

    const bool modifier_key_event = update_shortcut_modifier_state(steno, event);
    const bool shortcut_modifier_down = event->command
        || event->control
        || event->option
        || steno->command_down
        || steno->control_down
        || steno->option_down;

    const bool toggle_event = event->keycode == KEYCODE_ESCAPE
        && (event->control || steno->control_down || steno->toggle_esc_down);
    if (toggle_event) {
        if (event->is_down && !steno->toggle_esc_down) {
            steno->enabled = !steno->enabled;
            reset_chord(steno);
            fprintf(stderr, "stoin: steno capture %s\n", steno->enabled ? "enabled" : "disabled");
        }
        steno->toggle_esc_down = event->is_down;
        return true;
    }

    if (modifier_key_event || !steno->enabled || shortcut_modifier_down) {
        return false;
    }

    const Key_Binding *binding = keymap_find_binding(&steno->keymap, event->keycode);
    if (binding == NULL) {
        return false;
    }

    if (event->keycode >= 64) {
        return false;
    }

    const uint64_t physical_bit = keycode_physical_bit(event->keycode);
    if (event->is_down) {
        if ((steno->down_keycodes & physical_bit) == 0 && !event->is_repeat) {
            steno->down_keycodes |= physical_bit;
            steno->chord_bits |= binding->bits;
            if (steno->phrase_mode != STENO_PHRASE_MODE_NONE) {
                steno->chord_phrase_mode = steno->phrase_mode;
            }
        }
        return true;
    }

    steno->down_keycodes &= ~physical_bit;
    if (steno->down_keycodes == 0) {
        Stroke_Input stroke = {
            .bits = steno->chord_bits,
            .phrase_mode = steno->chord_phrase_mode,
            .phrase_namespace = steno->phrase_namespace_enabled,
        };
        (void)translate_completed_stroke_input(steno, stroke);
        reset_chord(steno);
    }
    return true;
}

void steno_set_phrase_namespace_enabled(Steno *steno, bool enabled)
{
    if (steno == NULL) {
        return;
    }
    steno->phrase_namespace_enabled = enabled;
    if (!enabled) {
        steno->phrase_mode = STENO_PHRASE_MODE_NONE;
        steno->chord_phrase_mode = STENO_PHRASE_MODE_NONE;
    }
}

void steno_set_phrase_mode(Steno *steno, bool active)
{
    steno_set_phrase_mode_family(steno, active ? STENO_PHRASE_MODE_ALL : STENO_PHRASE_MODE_NONE);
}

void steno_set_phrase_mode_family(Steno *steno, Steno_Phrase_Mode mode)
{
    if (steno == NULL) {
        return;
    }
    steno->phrase_mode = mode;
    if (mode != STENO_PHRASE_MODE_NONE && steno->down_keycodes != 0) {
        steno->chord_phrase_mode = mode;
    }
}

bool steno_handle_stroke(Steno *steno, Stroke_Input stroke)
{
    if (steno == NULL) {
        return false;
    }
    if (!steno->session_active) {
        return false;
    }
    return translate_completed_stroke_input(steno, stroke);
}

bool steno_handle_stroke_bits(Steno *steno, uint64_t bits)
{
    if (steno == NULL) {
        return false;
    }
    Stroke_Input stroke = {
        .bits = bits,
    };
    return steno_handle_stroke(steno, stroke);
}

void steno_set_session_active(Steno *steno, bool active)
{
    if (steno == NULL || steno->session_active == active) {
        return;
    }

    steno->session_active = active;
    reset_chord(steno);
    steno->toggle_esc_down = false;
    steno->control_down = false;
    steno->option_down = false;
    steno->command_down = false;
    steno->phrase_mode = STENO_PHRASE_MODE_NONE;
    steno->chord_phrase_mode = STENO_PHRASE_MODE_NONE;
}

size_t steno_key_binding_count(const Steno *steno)
{
    return steno == NULL ? 0 : keymap_binding_count(&steno->keymap);
}

size_t steno_dictionary_count(const Steno *steno)
{
    return steno == NULL ? 0 : dictionary_count(&steno->dictionary_stack.dictionary);
}

size_t steno_translation_history_stroke_count(const Steno *steno)
{
    return steno == NULL ? 0 : translation_history_stroke_count(steno->translations);
}

bool steno_lookup_stroke(Steno *steno, const char *stroke, const char **out_translation)
{
    if (steno == NULL) {
        return false;
    }
    uint64_t bits = 0;
    if (stroke != NULL && strchr(stroke, '/') == NULL && stroke_string_to_bits(stroke, &bits)) {
        char *phrase = NULL;
        const Phrase_Lookup_Result result = phrasing_lookup(steno->phrasing, bits, &phrase);
        if (result == PHRASE_LOOKUP_ERROR) {
            free(phrase);
            return false;
        }
        if (result == PHRASE_LOOKUP_HIT) {
            free(steno->lookup_translation);
            steno->lookup_translation = phrase;
            if (out_translation != NULL) {
                *out_translation = steno->lookup_translation;
            }
            return true;
        }
    }
    return dictionary_lookup_stroke(&steno->dictionary_stack.dictionary, stroke, out_translation);
}

bool steno_dump_dictionary_json(const Steno *steno, const char *path)
{
    if (steno == NULL) {
        return false;
    }
    return dictionary_dump_json(&steno->dictionary_stack.dictionary, path);
}
