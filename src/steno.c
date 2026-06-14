#include "steno.h"

#include "dictionary_stack.h"
#include "format.h"
#include "orthography.h"
#include "retro.h"
#include "steno_stroke.h"
#include "text_util.h"
#include "translation_history.h"
#include "util.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Key_Binding {
    uint16_t keycode;
    uint64_t bits;
} Key_Binding;

enum {
    MAX_TRANSLATION_STROKES = 100,
    TRANSLATION_COMPACT_INTERVAL_STROKES = 1000,
    TRANSLATION_HISTORY_STROKE_LIMIT = 1000,
    MAX_TRANSLATION_OUTLINE_BYTES = 4096,
};

struct Steno {
    Key_Binding *bindings;
    Translation *translations;
    Dictionary_Stack dictionary_stack;
    Orthography orthography;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    size_t strokes_since_compaction;
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
};

static bool translate_chord_bits(Steno *steno, uint64_t bits);

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
    steno->spacing.mode = SPACING_MODE_AFTER_WORD;
    return true;
}

static const char *steno_spacing(const Steno *steno)
{
    return steno != NULL && steno->spacing.spacing != NULL ? steno->spacing.spacing : "";
}

static bool load_keymap(Steno *steno, const char *path)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read keymap '%s'\n", path);
        return false;
    }

    char *cursor = file;
    int line_number = 1;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        while (isspace((unsigned char)*line)) {
            ++line;
        }
        if (*line == '\0' || (line[0] == '/' && line[1] == '/')) {
            ++line_number;
            continue;
        }

        char key_name[64] = {0};
        char steno_name[32] = {0};
        if (sscanf(line, "%63s %31s", key_name, steno_name) != 2) {
            fprintf(stderr, "stoin: invalid keymap line %d: %s\n", line_number, line);
            free(file);
            return false;
        }

        uint16_t keycode = 0;
        uint64_t bits = 0;
        if (!platform_keycode_from_name(key_name, &keycode)) {
            fprintf(stderr, "stoin: unknown key name on keymap line %d: %s\n", line_number, key_name);
            free(file);
            return false;
        }
        if (!stroke_string_to_bits(steno_name, &bits)) {
            fprintf(stderr, "stoin: invalid steno stroke on keymap line %d: %s\n", line_number, steno_name);
            free(file);
            return false;
        }

        Key_Binding binding = {
            .keycode = keycode,
            .bits = bits,
        };
        arrput(steno->bindings, binding);
        ++line_number;
    }

    free(file);
    return arrlenu(steno->bindings) > 0;
}

static const Key_Binding *find_binding(const Steno *steno, uint16_t keycode)
{
    for (size_t i = 0; i < arrlenu(steno->bindings); ++i) {
        if (steno->bindings[i].keycode == keycode) {
            return &steno->bindings[i];
        }
    }
    return NULL;
}

static void reset_chord(Steno *steno)
{
    steno->down_keycodes = 0;
    steno->chord_bits = 0;
}

enum {
    KEYCODE_ESCAPE = 53,
    KEYCODE_LEFT_COMMAND = 55,
    KEYCODE_RIGHT_COMMAND = 54,
    KEYCODE_LEFT_OPTION = 58,
    KEYCODE_RIGHT_OPTION = 61,
    KEYCODE_LEFT_CONTROL = 59,
    KEYCODE_RIGHT_CONTROL = 62,
};

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

typedef struct Text_Token {
    size_t start;
    size_t core_end;
    size_t end;
} Text_Token;

static bool stitch_is_word_byte(unsigned char c)
{
    return c >= 0x80 || isalnum(c) || c == '_' || c == '\'';
}

static bool collect_text_tokens(const char *text, Text_Token **out_tokens)
{
    if (text == NULL || out_tokens == NULL) {
        return false;
    }

    const size_t length = strlen(text);
    size_t index = 0;
    while (index < length) {
        while (index < length && isspace((unsigned char)text[index])) {
            ++index;
        }
        if (index >= length) {
            break;
        }

        const size_t start = index;
        if (stitch_is_word_byte((unsigned char)text[index])) {
            ++index;
            while (index < length) {
                const unsigned char c = (unsigned char)text[index];
                if (stitch_is_word_byte(c) || c == '-') {
                    ++index;
                } else {
                    break;
                }
            }
        } else {
            ++index;
            while (index < length
                && !isspace((unsigned char)text[index])
                && !stitch_is_word_byte((unsigned char)text[index])) {
                ++index;
            }
        }

        const size_t core_end = index;
        while (index < length && isspace((unsigned char)text[index])) {
            ++index;
        }
        arrput(*out_tokens, ((Text_Token) {
            .start = start,
            .core_end = core_end,
            .end = index,
        }));
    }

    return true;
}

static bool append_stitched_core(char **out, const char *start, const char *end, const char *delimiter)
{
    bool first = true;
    for (const char *p = start; p < end;) {
        const size_t length = utf8_codepoint_length(p, end);
        if (!first && !text_append_range(out, delimiter, strlen(delimiter))) {
            return false;
        }
        if (!text_append_range(out, p, length)) {
            return false;
        }
        p += length;
        first = false;
    }
    return true;
}

static bool stitch_text_suffix(
    const char *text,
    size_t token_count,
    const char *delimiter,
    bool phrase,
    char **out
)
{
    Text_Token *tokens = NULL;
    if (!collect_text_tokens(text, &tokens)) {
        return false;
    }

    const size_t available = arrlenu(tokens);
    if (available == 0 || token_count == 0) {
        arrfree(tokens);
        return text_append_cstring(out, text);
    }

    if (token_count > available) {
        token_count = available;
    }

    const size_t first_token = available - token_count;
    const size_t prefix_end = tokens[first_token].start;
    if (!text_append_range(out, text, prefix_end)) {
        arrfree(tokens);
        return false;
    }

    if (phrase) {
        for (size_t i = first_token; i < available; ++i) {
            if (i != first_token && !text_append_range(out, delimiter, strlen(delimiter))) {
                arrfree(tokens);
                return false;
            }
            if (!text_append_range(out, text + tokens[i].start, tokens[i].core_end - tokens[i].start)) {
                arrfree(tokens);
                return false;
            }
        }
        const Text_Token last = tokens[available - 1];
        if (!text_append_range(out, text + last.core_end, last.end - last.core_end)) {
            arrfree(tokens);
            return false;
        }
    } else {
        for (size_t i = first_token; i < available; ++i) {
            const Text_Token token = tokens[i];
            if (!append_stitched_core(out, text + token.start, text + token.core_end, delimiter)
                || !text_append_range(out, text + token.core_end, token.end - token.core_end)) {
                arrfree(tokens);
                return false;
            }
        }
    }

    arrfree(tokens);
    return true;
}

static size_t stitch_token_count(const char *text)
{
    Text_Token *tokens = NULL;
    if (!collect_text_tokens(text, &tokens)) {
        return 0;
    }
    const size_t count = arrlenu(tokens);
    arrfree(tokens);
    return count;
}

static size_t text_length_without_trailing_space(const Steno *steno, const char *text)
{
    size_t length = strlen(text);
    const char *spacing = steno_spacing(steno);
    const size_t spacing_length = strlen(spacing);
    if (steno->spacing.mode == SPACING_MODE_AFTER_WORD
        && spacing_length > 0
        && length >= spacing_length
        && memcmp(text + length - spacing_length, spacing, spacing_length) == 0) {
        length -= spacing_length;
    }
    return length;
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

static char *build_emitted_text(Steno *steno, const char *old_text, const Formatted_Text *formatted)
{
    char *emitted = NULL;
    const size_t old_text_length = old_text == NULL ? 0 : text_length_without_trailing_space(steno, old_text);
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

    const bool has_visible_text = emitted != NULL && emitted[0] != '\0';
    const char *spacing = steno_spacing(steno);
    const size_t spacing_length = strlen(spacing);
    if (has_visible_text
        && !formatted->attach_next
        && steno->spacing.mode == SPACING_MODE_AFTER_WORD
        && spacing_length > 0
        && !text_append_range(&emitted, spacing, spacing_length)) {
        arrfree(emitted);
        return NULL;
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

    arrsetlen(steno->translations, translation_count - 1);
    for (size_t i = 0; i < arrlenu(translation.replaced); ++i) {
        arrput(steno->translations, translation.replaced[i]);
    }

    arrsetlen(translation.replaced, 0);
    translation_destroy(&translation);
    arrfree(replacement_text);
    return true;
}

static bool retro_replace_output_callback(void *userdata, const char *old_text, const char *new_text)
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
    return (Retro_Context) {
        .translations = &steno->translations,
        .spacing = &steno->spacing,
        .replace_output = retro_replace_output_callback,
        .undo_last_translation = retro_undo_last_translation_callback,
        .translate_bits = retro_translate_bits_callback,
        .userdata = steno,
    };
}

static bool repeat_last_translation(Steno *steno, const uint64_t *strokes, size_t stroke_count)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return true;
    }

    const Translation *last = &steno->translations[translation_count - 1];
    Translation next = {
        .glue = last->glue,
        .next_attach = last->next_attach,
    };
    if (!text_append_cstring(&next.utf8, last->utf8)
        || !translation_set_strokes(&next, strokes, stroke_count)) {
        translation_destroy(&next);
        return false;
    }

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

static void trace_stroke(const Steno *steno, const char *raw_chord, const char *translation)
{
    if (steno == NULL || steno->trace_file == NULL || raw_chord == NULL) {
        return;
    }

    if (translation != NULL) {
        fprintf(steno->trace_file, "%s -> %s\n", raw_chord, translation);
    } else {
        fprintf(steno->trace_file, "%s -> [untranslated]\n", raw_chord);
    }
    fflush(steno->trace_file);
}

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

typedef struct Translation_Match {
    const char *translation;
    uint64_t strokes[MAX_TRANSLATION_STROKES];
    size_t stroke_count;
    size_t replaced_count;
    char outline[MAX_TRANSLATION_OUTLINE_BYTES];
} Translation_Match;

static void set_translation_match(
    Translation_Match *match,
    const char *translation,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count
)
{
    match->translation = translation;
    match->stroke_count = stroke_count;
    match->replaced_count = replaced_count;
    memcpy(match->strokes, strokes, stroke_count * sizeof(strokes[0]));
    (void)stroke_sequence_to_string(
        strokes,
        stroke_count,
        match->outline,
        sizeof(match->outline)
    );
}

static size_t effective_lookup_stroke_limit(const Steno *steno)
{
    size_t max_strokes = dictionary_longest_key(&steno->dictionary_stack.dictionary);
    if (max_strokes == 0 || max_strokes > MAX_TRANSLATION_STROKES) {
        max_strokes = MAX_TRANSLATION_STROKES;
    }
    return max_strokes;
}

static bool find_translation_match(Steno *steno, uint64_t bits, Translation_Match *out_match)
{
    if (steno == NULL || out_match == NULL) {
        return false;
    }

    const size_t max_strokes = effective_lookup_stroke_limit(steno);

    uint64_t candidate[MAX_TRANSLATION_STROKES] = { bits };
    size_t candidate_count = 1;
    size_t replaced_count = 0;
    bool found = false;

    const char *translation = dictionary_lookup_strokes(&steno->dictionary_stack.dictionary, candidate, candidate_count);
    if (translation != NULL && translation[0] != '=') {
        set_translation_match(out_match, translation, candidate, candidate_count, replaced_count);
        found = true;
    }

    for (size_t i = arrlenu(steno->translations); i > 0 && candidate_count < max_strokes;) {
        --i;
        const Translation *previous = &steno->translations[i];
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

        translation = dictionary_lookup_strokes(&steno->dictionary_stack.dictionary, candidate, candidate_count);
        if (translation != NULL && translation[0] != '=') {
            set_translation_match(out_match, translation, candidate, candidate_count, replaced_count);
            found = true;
        }
    }

    if (!found) {
        set_translation_match(out_match, NULL, &bits, 1, 0);
    }
    return true;
}

static bool apply_stitch_retro_match(
    Steno *steno,
    const Translation_Match *match,
    const Formatted_Text *formatted
)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        return false;
    }

    size_t replace_start = translation_count - match->replaced_count;
    char *source_text = NULL;
    while (true) {
        arrfree(source_text);
        source_text = translation_range_source_text(
            steno->translations,
            replace_start,
            translation_count - replace_start
        );
        if (source_text == NULL) {
            return false;
        }
        if (stitch_token_count(source_text) >= formatted->stitch_count || replace_start == 0) {
            break;
        }
        --replace_start;
    }

    const size_t replaced_count = translation_count - replace_start;
    char *old_text = translation_range_text(steno->translations, replace_start, replaced_count);
    char *new_text = NULL;
    if (old_text == NULL
        || !stitch_text_suffix(
            source_text,
            formatted->stitch_count,
            formatted->stitch_delimiter != NULL ? formatted->stitch_delimiter : "-",
            formatted->stitch_phrase,
            &new_text)) {
        arrfree(old_text);
        arrfree(source_text);
        arrfree(new_text);
        return false;
    }

    Translation next = {0};
    next.utf8 = new_text;
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        arrfree(source_text);
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)) {
        arrfree(old_text);
        arrfree(source_text);
        translation_destroy(&next);
        return false;
    }

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, steno->translations[i]);
    }
    arrsetlen(steno->translations, replace_start);
    arrput(steno->translations, next);

    arrfree(old_text);
    arrfree(source_text);
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

    if (formatted.stitch_last_word || formatted.stitch_phrase) {
        const bool ok = apply_stitch_retro_match(steno, match, &formatted);
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

    if (formatted.cancel_formatting) {
        steno->next_case = CASE_MODE_NORMAL;
    }
    if (formatted.carry_case
        && formatted.next_case == CASE_MODE_NORMAL
        && steno->next_case != CASE_MODE_NORMAL) {
        formatted.next_case = steno->next_case;
    }
    if (formatted.text[0] != '\0') {
        formatted_text_apply_case(formatted.text, steno->case_mode);
        const Case_Mode text_case = formatted.text_case != CASE_MODE_NORMAL
            ? formatted.text_case
            : steno->next_case;
        formatted_text_apply_case(formatted.text, text_case);
        steno->next_case = formatted.next_case;
    } else if (formatted.next_case != CASE_MODE_NORMAL) {
        steno->next_case = formatted.next_case;
    }

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
    next.utf8 = build_emitted_text(steno, old_text, &formatted);
    next.glue = formatted.glue;
    next.next_attach = formatted.attach_next;

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

bool steno_get_dictionary_paths(const Steno *steno, const char *const **out_paths, size_t *out_path_count)
{
    return dictionary_stack_get_paths(
        steno == NULL ? NULL : &steno->dictionary_stack,
        out_paths,
        out_path_count
    );
}

static bool translate_chord_bits(Steno *steno, uint64_t bits)
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
        trace_stroke(steno, raw_chord, single_stroke_translation);
        const bool ok = execute_command(steno, single_stroke_translation, &bits, 1);
        if (ok) {
            count_completed_stroke(steno);
        }
        return ok;
    }

    Translation_Match match = {0};
    if (!find_translation_match(steno, bits, &match)) {
        return false;
    }

    trace_stroke(steno, match.outline, match.translation);
    const bool ok = apply_translation_match(steno, &match);
    if (ok) {
        count_completed_stroke(steno);
    }
    return ok;
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
        || (config->keymap_path != NULL && !load_keymap(steno, config->keymap_path))) {
        steno_destroy(steno);
        return NULL;
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

    arrfree(steno->bindings);
    for (size_t i = 0; i < arrlenu(steno->translations); ++i) {
        translation_destroy(&steno->translations[i]);
    }
    arrfree(steno->translations);
    orthography_destroy(&steno->orthography);
    dictionary_stack_destroy(&steno->dictionary_stack);
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

    const Key_Binding *binding = find_binding(steno, event->keycode);
    if (binding == NULL) {
        return false;
    }

    if (event->keycode >= 64) {
        return false;
    }

    const uint64_t physical_bit = UINT64_C(1) << event->keycode;
    if (event->is_down) {
        if ((steno->down_keycodes & physical_bit) == 0 && !event->is_repeat) {
            steno->down_keycodes |= physical_bit;
            steno->chord_bits |= binding->bits;
        }
        return true;
    }

    steno->down_keycodes &= ~physical_bit;
    if (steno->down_keycodes == 0) {
        (void)translate_chord_bits(steno, steno->chord_bits);
        reset_chord(steno);
    }
    return true;
}

bool steno_handle_stroke_bits(Steno *steno, uint64_t bits)
{
    if (steno == NULL) {
        return false;
    }
    if (!steno->session_active) {
        return false;
    }
    return translate_chord_bits(steno, bits);
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
}

size_t steno_key_binding_count(const Steno *steno)
{
    return steno == NULL ? 0 : arrlenu(steno->bindings);
}

size_t steno_dictionary_count(const Steno *steno)
{
    return steno == NULL ? 0 : dictionary_count(&steno->dictionary_stack.dictionary);
}

size_t steno_translation_history_stroke_count(const Steno *steno)
{
    return steno == NULL ? 0 : translation_history_stroke_count(steno->translations);
}

bool steno_lookup_stroke(const Steno *steno, const char *stroke, const char **out_translation)
{
    if (steno == NULL) {
        return false;
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
