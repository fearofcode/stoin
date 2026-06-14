#include "steno.h"

#include "dictionary.h"
#include "orthography.h"
#include "steno_stroke.h"
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

typedef struct Translation Translation;
struct Translation {
    uint64_t *strokes;
    char *utf8;
    Translation *replaced;
    bool glue;
    bool next_attach;
};

enum {
    MAX_TRANSLATION_STROKES = 100,
    TRANSLATION_COMPACT_INTERVAL_STROKES = 1000,
    TRANSLATION_HISTORY_STROKE_LIMIT = 1000,
    MAX_TRANSLATION_OUTLINE_BYTES = 4096,
};

struct Steno {
    Key_Binding *bindings;
    Translation *translations;
    char **dictionary_paths;
    Platform_File_Stamp *dictionary_stamps;
    Dictionary dictionary;
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
    bool dictionary_reload_error_reported;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    void *send_userdata;
    FILE *trace_file;
};

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

static bool file_stamps_equal(Platform_File_Stamp a, Platform_File_Stamp b)
{
    return a.exists == b.exists
        && a.size == b.size
        && a.modified_time_ns == b.modified_time_ns;
}

static void clear_dictionary_paths(Steno *steno)
{
    if (steno == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(steno->dictionary_paths); ++i) {
        free(steno->dictionary_paths[i]);
    }
    arrfree(steno->dictionary_paths);
    steno->dictionary_paths = NULL;
    arrfree(steno->dictionary_stamps);
    steno->dictionary_stamps = NULL;
}

static bool add_dictionary_path(Steno *steno, const char *path)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    arrput(steno->dictionary_paths, copy);
    return true;
}

static bool set_dictionary_paths_from_config(Steno *steno, const Steno_Config *config)
{
    if (steno == NULL || config == NULL) {
        return false;
    }

    clear_dictionary_paths(steno);
    if (config->dictionary_path_count > 0) {
        if (config->dictionary_paths == NULL) {
            return false;
        }
        for (size_t i = 0; i < config->dictionary_path_count; ++i) {
            if (config->dictionary_paths[i] == NULL
                || !add_dictionary_path(steno, config->dictionary_paths[i])) {
                clear_dictionary_paths(steno);
                return false;
            }
        }
    } else {
        if (config->dictionary_path == NULL || !add_dictionary_path(steno, config->dictionary_path)) {
            clear_dictionary_paths(steno);
            return false;
        }
    }

    return arrlenu(steno->dictionary_paths) > 0;
}

static bool refresh_dictionary_stamps(Steno *steno)
{
    if (steno == NULL) {
        return false;
    }

    arrsetlen(steno->dictionary_stamps, arrlenu(steno->dictionary_paths));
    for (size_t i = 0; i < arrlenu(steno->dictionary_paths); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(steno->dictionary_paths[i], &stamp)) {
            arrsetlen(steno->dictionary_stamps, 0);
            return false;
        }
        steno->dictionary_stamps[i] = stamp;
    }
    return true;
}

static bool dictionary_files_changed(Steno *steno, bool *out_changed)
{
    if (steno == NULL || out_changed == NULL) {
        return false;
    }

    *out_changed = false;
    if (arrlenu(steno->dictionary_stamps) != arrlenu(steno->dictionary_paths)) {
        *out_changed = true;
        return true;
    }

    for (size_t i = 0; i < arrlenu(steno->dictionary_paths); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(steno->dictionary_paths[i], &stamp)) {
            return false;
        }
        if (!file_stamps_equal(stamp, steno->dictionary_stamps[i])) {
            *out_changed = true;
            return true;
        }
    }

    return true;
}

static bool load_dictionary_from_paths(Dictionary *dictionary, char *const *paths, size_t path_count)
{
    return dictionary_load_many(dictionary, (const char *const *)paths, path_count);
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

typedef struct Formatted_Text {
    char *text;
    char *ortho_suffix;
    char *stitch_delimiter;
    size_t stitch_count;
    size_t ortho_suffix_text_offset;
    size_t ortho_suffix_text_length;
    bool attach_prev;
    bool attach_next;
    bool glue;
    bool stitch;
    bool stitch_last_word;
    bool stitch_phrase;
} Formatted_Text;

static bool array_string_append_char(char **out, char c)
{
    if (out == NULL) {
        return false;
    }

    if (*out != NULL && arrlenu(*out) > 0) {
        arrpop(*out);
    }
    arrput(*out, c);
    arrput(*out, '\0');
    return true;
}

static bool array_string_append_range(char **out, const char *start, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!array_string_append_char(out, start[i])) {
            return false;
        }
    }
    return true;
}

static bool array_string_prepend_range(char **out, const char *start, size_t length)
{
    if (out == NULL || start == NULL) {
        return false;
    }

    char *result = NULL;
    if (!array_string_append_range(&result, start, length)
        || (*out != NULL && !array_string_append_range(&result, *out, strlen(*out)))) {
        arrfree(result);
        return false;
    }

    arrfree(*out);
    *out = result;
    return true;
}

static bool is_digit_string(const char *s)
{
    if (s == NULL || s[0] == '\0') {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        if (!isdigit(*p)) {
            return false;
        }
    }
    return true;
}

static bool append_escaped_translation_byte(char **out, const char **cursor)
{
    const char *p = *cursor;
    if (*p != '\\') {
        const bool ok = array_string_append_char(out, *p);
        *cursor = p + 1;
        return ok;
    }

    ++p;
    char c = *p;
    if (c == '\0') {
        c = '\\';
        *cursor = p;
    } else {
        switch (c) {
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        default: break;
        }
        *cursor = p + 1;
    }

    return array_string_append_char(out, c);
}

static bool parse_attach_meta(Formatted_Text *formatted, const char *text, size_t length)
{
    bool begin = length > 0 && text[0] == '^';
    bool end = length > 0 && text[length - 1] == '^';
    if (!begin && !end) {
        begin = true;
        end = true;
    }

    size_t start = begin ? 1 : 0;
    size_t end_index = length - (end ? 1 : 0);
    if (start > end_index) {
        start = end_index;
    }

    if (begin) {
        formatted->attach_prev = true;
    }
    if (end) {
        formatted->attach_next = true;
    }
    return array_string_append_range(&formatted->text, text + start, end_index - start);
}

static bool parse_ortho_attach_meta(Formatted_Text *formatted, const char *text, size_t length)
{
    if (formatted == NULL || text == NULL || length < 2 || text[0] != '^' || text[length - 1] == '^') {
        return false;
    }

    formatted->attach_prev = true;
    const char *suffix = text + 1;
    const size_t suffix_length = length - 1;
    if (formatted->ortho_suffix == NULL) {
        formatted->ortho_suffix = copy_range(suffix, suffix_length);
        if (formatted->ortho_suffix == NULL) {
            return false;
        }
        formatted->ortho_suffix_text_offset = formatted->text == NULL ? 0 : strlen(formatted->text);
        formatted->ortho_suffix_text_length = suffix_length;
    }
    return array_string_append_range(&formatted->text, suffix, suffix_length);
}

static bool is_attach_meta(const char *text, size_t length)
{
    return length > 0
        && memchr(text, '~', length) == NULL
        && memchr(text, '|', length) == NULL
        && (text[0] == '^' || text[length - 1] == '^');
}

static bool meta_starts_with(const char *meta, size_t meta_length, const char *prefix)
{
    const size_t prefix_length = strlen(prefix);
    return meta_length >= prefix_length && memcmp(meta, prefix, prefix_length) == 0;
}

static bool formatted_set_stitch_delimiter(Formatted_Text *formatted, const char *delimiter, size_t delimiter_length)
{
    char *copy = copy_range(delimiter, delimiter_length);
    if (copy == NULL) {
        return false;
    }
    free(formatted->stitch_delimiter);
    formatted->stitch_delimiter = copy;
    return true;
}

static bool parse_positive_size(const char *start, size_t length, size_t *out)
{
    if (out == NULL || length == 0) {
        return false;
    }

    size_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)start[i])) {
            return false;
        }
        value = value * 10 + (size_t)(start[i] - '0');
    }
    if (value == 0) {
        return false;
    }

    *out = value;
    return true;
}

static bool parse_stitch_meta(Formatted_Text *formatted, const char *meta_start, size_t meta_length)
{
    const char prefix[] = ":stitch:";
    if (!meta_starts_with(meta_start, meta_length, prefix)) {
        return false;
    }

    const char *args = meta_start + strlen(prefix);
    const size_t args_length = meta_length - strlen(prefix);
    const char *delimiter = "-";
    size_t delimiter_length = 1;
    const char *separator = memchr(args, ':', args_length);
    const size_t word_length = separator == NULL ? args_length : (size_t)(separator - args);
    if (word_length == 0) {
        return false;
    }
    if (separator != NULL) {
        delimiter = separator + 1;
        delimiter_length = args_length - word_length - 1;
    }

    formatted->stitch = true;
    formatted->glue = true;
    return formatted_set_stitch_delimiter(formatted, delimiter, delimiter_length)
        && array_string_append_range(&formatted->text, args, word_length);
}

static bool parse_stitch_retro_meta(
    Formatted_Text *formatted,
    const char *meta_start,
    size_t meta_length,
    const char *command,
    bool phrase
)
{
    const size_t command_length = strlen(command);
    if (!meta_starts_with(meta_start, meta_length, command)) {
        return false;
    }
    if (meta_length > command_length && meta_start[command_length] != ':') {
        return false;
    }

    size_t count = 1;
    const char *delimiter = "-";
    size_t delimiter_length = 1;
    const char *args = meta_start + command_length;
    size_t args_length = meta_length - command_length;
    if (args_length > 0) {
        ++args;
        --args_length;

        const char *separator = memchr(args, ':', args_length);
        const size_t count_length = separator == NULL ? args_length : (size_t)(separator - args);
        if (count_length > 0 && !parse_positive_size(args, count_length, &count)) {
            return false;
        }
        if (separator != NULL) {
            delimiter = separator + 1;
            delimiter_length = args_length - count_length - 1;
        }
    }

    formatted->stitch_count = count;
    formatted->stitch_last_word = !phrase;
    formatted->stitch_phrase = phrase;
    return formatted_set_stitch_delimiter(formatted, delimiter, delimiter_length);
}

static bool apply_translation_meta(
    Formatted_Text *formatted,
    const char *translation,
    const char *meta_start,
    size_t meta_length,
    bool *pending_attach_prev
)
{
    if (parse_stitch_meta(formatted, meta_start, meta_length)
        || parse_stitch_retro_meta(formatted, meta_start, meta_length, ":stitch_last_word", false)
        || parse_stitch_retro_meta(formatted, meta_start, meta_length, ":stitch_phrase", true)) {
        return true;
    }

    if (meta_length >= 5 && memcmp(meta_start, "glue:", 5) == 0) {
        formatted->glue = true;
        return array_string_append_range(&formatted->text, meta_start + 5, meta_length - 5);
    }
    if (meta_length >= 6 && memcmp(meta_start, ":glue:", 6) == 0) {
        formatted->glue = true;
        return array_string_append_range(&formatted->text, meta_start + 6, meta_length - 6);
    }
    if (meta_length >= 1 && meta_start[0] == '&') {
        formatted->glue = true;
        return array_string_append_range(&formatted->text, meta_start + 1, meta_length - 1);
    }

    const char *attach = meta_start;
    size_t attach_length = meta_length;
    if (meta_length == 7 && memcmp(meta_start, ":attach", 7) == 0) {
        attach = meta_start + 7;
        attach_length = 0;
    } else if (meta_length >= 8 && memcmp(meta_start, ":attach:", 8) == 0) {
        attach = meta_start + 8;
        attach_length = meta_length - 8;
    } else if (!is_attach_meta(meta_start, meta_length)) {
        if (*pending_attach_prev) {
            formatted->attach_prev = true;
            *pending_attach_prev = false;
        }
        return array_string_append_range(&formatted->text, translation, meta_length + 2);
    }

    if (attach_length == 1 && attach[0] == '^') {
        if (formatted->text != NULL && formatted->text[0] != '\0') {
            formatted->attach_next = true;
        } else {
            *pending_attach_prev = true;
        }
        return true;
    }

    if (*pending_attach_prev) {
        formatted->attach_prev = true;
        *pending_attach_prev = false;
    }
    if (parse_ortho_attach_meta(formatted, attach, attach_length)) {
        return true;
    }
    return parse_attach_meta(formatted, attach, attach_length);
}

static bool format_translation_text(const char *translation, Formatted_Text *out)
{
    if (translation == NULL || out == NULL) {
        return false;
    }

    bool pending_attach_prev = false;
    if (is_digit_string(translation)) {
        out->glue = true;
    }

    for (const char *p = translation; *p != '\0';) {
        if (*p == '\\') {
            if (pending_attach_prev) {
                out->attach_prev = true;
                pending_attach_prev = false;
            }
            if (!append_escaped_translation_byte(&out->text, &p)) {
                return false;
            }
            continue;
        }

        if (*p != '{') {
            if (pending_attach_prev) {
                out->attach_prev = true;
                pending_attach_prev = false;
            }
            if (!array_string_append_char(&out->text, *p++)) {
                return false;
            }
            continue;
        }

        const char *meta = p + 1;
        const char *end = meta;
        while (*end != '\0') {
            if (*end == '\\' && end[1] != '\0') {
                end += 2;
            } else if (*end == '}') {
                break;
            } else {
                ++end;
            }
        }

        if (*end != '}') {
            if (!array_string_append_char(&out->text, *p++)) {
                return false;
            }
            continue;
        }

        if (!apply_translation_meta(out, p, meta, (size_t)(end - meta), &pending_attach_prev)) {
            return false;
        }
        p = end + 1;
    }

    if (pending_attach_prev) {
        out->attach_prev = true;
        out->attach_next = true;
    }
    if (out->text == NULL) {
        arrput(out->text, '\0');
    }
    return true;
}

static void formatted_text_destroy(Formatted_Text *formatted)
{
    if (formatted == NULL) {
        return;
    }
    arrfree(formatted->text);
    free(formatted->ortho_suffix);
    free(formatted->stitch_delimiter);
    memset(formatted, 0, sizeof(*formatted));
}

static void translation_destroy(Translation *translation)
{
    if (translation == NULL) {
        return;
    }

    arrfree(translation->strokes);
    arrfree(translation->utf8);
    for (size_t i = 0; i < arrlenu(translation->replaced); ++i) {
        translation_destroy(&translation->replaced[i]);
    }
    arrfree(translation->replaced);
    memset(translation, 0, sizeof(*translation));
}

static bool translation_set_strokes(Translation *translation, const uint64_t *strokes, size_t stroke_count)
{
    if (translation == NULL || strokes == NULL || stroke_count == 0) {
        return false;
    }

    for (size_t i = 0; i < stroke_count; ++i) {
        arrput(translation->strokes, strokes[i]);
    }
    return true;
}

static bool append_string(char **out, const char *s)
{
    if (out == NULL || s == NULL) {
        return false;
    }

    if (*out != NULL && arrlenu(*out) > 0) {
        arrpop(*out);
    }
    for (const char *p = s; *p != '\0'; ++p) {
        arrput(*out, *p);
    }
    arrput(*out, '\0');
    return true;
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

static size_t utf8_codepoint_length(const char *s, const char *end)
{
    if (s >= end) {
        return 0;
    }

    const unsigned char c = (unsigned char)*s;
    size_t length = 1;
    if ((c & 0xE0) == 0xC0) {
        length = 2;
    } else if ((c & 0xF0) == 0xE0) {
        length = 3;
    } else if ((c & 0xF8) == 0xF0) {
        length = 4;
    }
    if ((size_t)(end - s) < length) {
        return 1;
    }
    return length;
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
        if (!first && !array_string_append_range(out, delimiter, strlen(delimiter))) {
            return false;
        }
        if (!array_string_append_range(out, p, length)) {
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
        return append_string(out, text);
    }

    if (token_count > available) {
        token_count = available;
    }

    const size_t first_token = available - token_count;
    const size_t prefix_end = tokens[first_token].start;
    if (!array_string_append_range(out, text, prefix_end)) {
        arrfree(tokens);
        return false;
    }

    if (phrase) {
        for (size_t i = first_token; i < available; ++i) {
            if (i != first_token && !array_string_append_range(out, delimiter, strlen(delimiter))) {
                arrfree(tokens);
                return false;
            }
            if (!array_string_append_range(out, text + tokens[i].start, tokens[i].core_end - tokens[i].start)) {
                arrfree(tokens);
                return false;
            }
        }
        const Text_Token last = tokens[available - 1];
        if (!array_string_append_range(out, text + last.core_end, last.end - last.core_end)) {
            arrfree(tokens);
            return false;
        }
    } else {
        for (size_t i = first_token; i < available; ++i) {
            const Text_Token token = tokens[i];
            if (!append_stitched_core(out, text + token.start, text + token.core_end, delimiter)
                || !array_string_append_range(out, text + token.core_end, token.end - token.core_end)) {
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
    if (steno->spacing.mode == SPACING_MODE_AFTER_WORD
        && steno->spacing.spacing_char != '\0'
        && length > 0
        && text[length - 1] == steno->spacing.spacing_char) {
        --length;
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

    if (!array_string_append_range(emitted, old_text, word_start)) {
        return false;
    }

    const size_t word_length = old_text_length - word_start;
    if (word_length == 0) {
        return array_string_append_range(emitted, formatted->text, strlen(formatted->text));
    }

    char *word = copy_range(old_text + word_start, word_length);
    char *joined = NULL;
    if (word == NULL || !orthography_apply(&steno->orthography, word, formatted->ortho_suffix, &joined)) {
        free(word);
        free(joined);
        return false;
    }

    const bool ok = array_string_append_range(emitted, formatted->text, formatted->ortho_suffix_text_offset)
        && array_string_append_range(emitted, joined, strlen(joined))
        && array_string_append_range(
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
            if (!array_string_append_range(&emitted, old_text, old_text_length)) {
                arrfree(emitted);
                return NULL;
            }
        }

        if (!array_string_append_range(&emitted, formatted->text, strlen(formatted->text))) {
            arrfree(emitted);
            return NULL;
        }
    }

    if (!formatted->attach_next
        && steno->spacing.mode == SPACING_MODE_AFTER_WORD
        && steno->spacing.spacing_char != '\0'
        && !array_string_append_char(&emitted, steno->spacing.spacing_char)) {
        arrfree(emitted);
        return NULL;
    }
    if (emitted == NULL) {
        arrput(emitted, '\0');
    }
    return emitted;
}

static char *translation_range_text(const Translation *translations, size_t start, size_t count)
{
    char *text = NULL;
    arrput(text, '\0');
    for (size_t i = 0; i < count; ++i) {
        if (!append_string(&text, translations[start + i].utf8)) {
            arrfree(text);
            return NULL;
        }
    }
    return text;
}

static char *translation_replaced_text(const Translation *translation)
{
    if (translation == NULL) {
        return NULL;
    }
    return translation_range_text(translation->replaced, 0, arrlenu(translation->replaced));
}

static char *translation_source_text(const Translation *translation);

static char *translation_range_source_text(const Translation *translations, size_t start, size_t count)
{
    char *text = NULL;
    arrput(text, '\0');
    for (size_t i = 0; i < count; ++i) {
        char *source = translation_source_text(&translations[start + i]);
        if (source == NULL || !append_string(&text, source)) {
            arrfree(source);
            arrfree(text);
            return NULL;
        }
        arrfree(source);
    }
    return text;
}

static char *translation_source_text(const Translation *translation)
{
    if (translation == NULL) {
        return NULL;
    }
    if (arrlenu(translation->replaced) > 0) {
        return translation_range_source_text(translation->replaced, 0, arrlenu(translation->replaced));
    }
    char *text = NULL;
    if (!append_string(&text, translation->utf8)) {
        arrfree(text);
        return NULL;
    }
    return text;
}

static size_t common_utf8_prefix_bytes(const char *a, const char *b)
{
    size_t index = 0;
    size_t last_boundary = 0;

    while (a[index] != '\0' && b[index] != '\0' && a[index] == b[index]) {
        ++index;
        if (((unsigned char)a[index] & 0xC0) != 0x80) {
            last_boundary = index;
        }
    }

    return last_boundary;
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

static bool execute_command(Steno *steno, const char *command)
{
    if (strcmp(command, "=undo") == 0) {
        return undo_last_translation(steno);
    }

    fprintf(stderr, "stoin: unknown dictionary command '%s'\n", command);
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
    size_t max_strokes = dictionary_longest_key(&steno->dictionary);
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

    const char *translation = dictionary_lookup_strokes(&steno->dictionary, candidate, candidate_count);
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

        translation = dictionary_lookup_strokes(&steno->dictionary, candidate, candidate_count);
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

    if (formatted.stitch_last_word || formatted.stitch_phrase) {
        const bool ok = apply_stitch_retro_match(steno, match, &formatted);
        formatted_text_destroy(&formatted);
        return ok;
    }

    size_t replaced_count = match->replaced_count;
    if (replaced_count == 0 && translation_count > 0) {
        const Translation *previous = &steno->translations[translation_count - 1];
        if (formatted.stitch && previous->glue) {
            if (!array_string_prepend_range(
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
    formatted_text_destroy(&formatted);

    if (next.utf8 == NULL) {
        arrfree(old_text);
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
                translation_destroy(&next);
                return false;
            }
        }
    }
    if (!translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, steno->translations[i]);
    }
    arrsetlen(steno->translations, replace_start);
    arrput(steno->translations, next);

    arrfree(old_text);
    return true;
}

static size_t translation_history_stroke_count(const Translation *translations)
{
    size_t stroke_count = 0;
    for (size_t i = 0; i < arrlenu(translations); ++i) {
        stroke_count += arrlenu(translations[i].strokes);
    }
    return stroke_count;
}

static void compact_translation_history(Steno *steno)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return;
    }

    size_t keep_strokes = TRANSLATION_HISTORY_STROKE_LIMIT;
    const size_t lookup_strokes = effective_lookup_stroke_limit(steno);
    if (keep_strokes < lookup_strokes) {
        keep_strokes = lookup_strokes;
    }

    size_t retained_strokes = 0;
    size_t retained_translations = 0;
    for (size_t i = translation_count; i > 0; --i) {
        ++retained_translations;
        retained_strokes += arrlenu(steno->translations[i - 1].strokes);
        if (retained_strokes >= keep_strokes) {
            break;
        }
    }

    const size_t dropped_translations = translation_count - retained_translations;
    if (dropped_translations == 0) {
        return;
    }

    for (size_t i = 0; i < dropped_translations; ++i) {
        translation_destroy(&steno->translations[i]);
    }
    memmove(
        steno->translations,
        steno->translations + dropped_translations,
        retained_translations * sizeof(steno->translations[0])
    );
    arrsetlen(steno->translations, retained_translations);
}

static void count_completed_stroke(Steno *steno)
{
    ++steno->strokes_since_compaction;
    if (steno->strokes_since_compaction >= TRANSLATION_COMPACT_INTERVAL_STROKES) {
        compact_translation_history(steno);
        steno->strokes_since_compaction = 0;
    }
}

bool steno_reload_dictionary(Steno *steno)
{
    if (steno == NULL || arrlenu(steno->dictionary_paths) == 0) {
        return false;
    }

    Dictionary next = {0};
    if (!load_dictionary_from_paths(&next, steno->dictionary_paths, arrlenu(steno->dictionary_paths))) {
        dictionary_destroy(&next);
        (void)refresh_dictionary_stamps(steno);
        if (!steno->dictionary_reload_error_reported) {
            fputs("stoin: dictionary changed but reload failed; keeping previous dictionary\n", stderr);
            steno->dictionary_reload_error_reported = true;
        }
        return false;
    }

    dictionary_destroy(&steno->dictionary);
    steno->dictionary = next;
    if (!refresh_dictionary_stamps(steno)) {
        fputs("stoin: warning: reloaded dictionary, but failed to refresh dictionary file stamps\n", stderr);
    }

    steno->dictionary_reload_error_reported = false;
    fprintf(stderr, "stoin: reloaded %zu dictionary entries\n", dictionary_count(&steno->dictionary));
    return true;
}

bool steno_reload_dictionary_if_changed(Steno *steno)
{
    if (steno == NULL) {
        return false;
    }

    bool changed = false;
    if (!dictionary_files_changed(steno, &changed)) {
        if (!steno->dictionary_reload_error_reported) {
            fputs("stoin: failed to check dictionary files for changes\n", stderr);
            steno->dictionary_reload_error_reported = true;
        }
        return false;
    }

    if (!changed) {
        return true;
    }
    steno->dictionary_reload_error_reported = false;
    return steno_reload_dictionary(steno);
}

bool steno_get_dictionary_paths(const Steno *steno, const char *const **out_paths, size_t *out_path_count)
{
    if (steno == NULL || out_paths == NULL || out_path_count == NULL) {
        return false;
    }

    *out_paths = (const char *const *)steno->dictionary_paths;
    *out_path_count = arrlenu(steno->dictionary_paths);
    return true;
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

    const char *single_stroke_translation = dictionary_lookup_bits(&steno->dictionary, bits);
    if (single_stroke_translation != NULL && single_stroke_translation[0] == '=') {
        trace_stroke(steno, raw_chord, single_stroke_translation);
        const bool ok = execute_command(steno, single_stroke_translation);
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
    steno->spacing = (Spacing_State) {
        .mode = SPACING_MODE_AFTER_WORD,
        .spacing_char = ' ',
    };
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_userdata = config->send_userdata;
    steno->trace_file = config->trace_file;

    if (config->keymap_path != NULL && !load_keymap(steno, config->keymap_path)) {
        steno_destroy(steno);
        return NULL;
    }

    if (!set_dictionary_paths_from_config(steno, config)
        || !load_dictionary_from_paths(
            &steno->dictionary,
            steno->dictionary_paths,
            arrlenu(steno->dictionary_paths))) {
        steno_destroy(steno);
        return NULL;
    }
    if (!refresh_dictionary_stamps(steno)) {
        fputs("stoin: warning: failed to capture dictionary file stamps; hot reload may not work\n", stderr);
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
    clear_dictionary_paths(steno);
    for (size_t i = 0; i < arrlenu(steno->translations); ++i) {
        translation_destroy(&steno->translations[i]);
    }
    arrfree(steno->translations);
    orthography_destroy(&steno->orthography);
    dictionary_destroy(&steno->dictionary);
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
    return steno == NULL ? 0 : dictionary_count(&steno->dictionary);
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
    return dictionary_lookup_stroke(&steno->dictionary, stroke, out_translation);
}

bool steno_dump_dictionary_json(const Steno *steno, const char *path)
{
    if (steno == NULL) {
        return false;
    }
    return dictionary_dump_json(&steno->dictionary, path);
}
