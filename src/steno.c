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

typedef enum Case_Mode {
    CASE_MODE_NORMAL,
    CASE_MODE_CAP_FIRST_WORD,
    CASE_MODE_UPPER_FIRST_WORD,
    CASE_MODE_LOWER_FIRST_CHAR,
} Case_Mode;

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
    bool *dictionary_enabled;
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
    Case_Mode next_case;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    Send_Key_Combination_Fn send_key_combination;
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
    arrfree(steno->dictionary_enabled);
    steno->dictionary_enabled = NULL;
    arrfree(steno->dictionary_stamps);
    steno->dictionary_stamps = NULL;
}

static bool add_dictionary_path(Steno *steno, const char *path, bool enabled)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    arrput(steno->dictionary_paths, copy);
    arrput(steno->dictionary_enabled, enabled);
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
            const bool enabled = config->dictionary_enabled == NULL || config->dictionary_enabled[i];
            if (config->dictionary_paths[i] == NULL
                || !add_dictionary_path(steno, config->dictionary_paths[i], enabled)) {
                clear_dictionary_paths(steno);
                return false;
            }
        }
    } else {
        if (config->dictionary_path == NULL || !add_dictionary_path(steno, config->dictionary_path, true)) {
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

static bool load_dictionary_from_paths(
    Dictionary *dictionary,
    char *const *paths,
    const bool *enabled,
    size_t path_count
)
{
    if (dictionary == NULL || paths == NULL || enabled == NULL || path_count == 0) {
        return false;
    }

    bool loaded_any = false;
    for (size_t i = 0; i < path_count; ++i) {
        if (!enabled[i]) {
            continue;
        }
        if (paths[i] == NULL || !dictionary_load(dictionary, paths[i])) {
            return false;
        }
        loaded_any = true;
    }

    if (!loaded_any) {
        sh_new_strdup(dictionary->entries);
    }
    return true;
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
    char **key_combos;
    char *plover_command;
    size_t stitch_count;
    size_t ortho_suffix_text_offset;
    size_t ortho_suffix_text_length;
    Case_Mode text_case;
    Case_Mode next_case;
    Case_Mode retro_case;
    bool attach_prev;
    bool attach_next;
    bool glue;
    bool stitch;
    bool stitch_last_word;
    bool stitch_phrase;
    bool carry_case;
    bool cancel_formatting;
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

static bool ascii_equals_ignore_case(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool ascii_range_equals_ignore_case(const char *a, size_t a_length, const char *b)
{
    if (a == NULL || b == NULL || strlen(b) != a_length) {
        return false;
    }
    for (size_t i = 0; i < a_length; ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static bool ascii_range_starts_with_ignore_case(const char *s, size_t length, const char *prefix)
{
    if (s == NULL || prefix == NULL) {
        return false;
    }
    const size_t prefix_length = strlen(prefix);
    return length >= prefix_length && ascii_range_equals_ignore_case(s, prefix_length, prefix);
}

static bool is_word_byte(unsigned char c)
{
    return c >= 0x80 || isalnum(c) || c == '_';
}

static void apply_case_mode_to_text(char *text, Case_Mode mode)
{
    if (text == NULL || mode == CASE_MODE_NORMAL) {
        return;
    }

    if (mode == CASE_MODE_UPPER_FIRST_WORD) {
        bool in_word = false;
        for (char *p = text; *p != '\0'; ++p) {
            const unsigned char c = (unsigned char)*p;
            if (!in_word) {
                if (!is_word_byte(c)) {
                    continue;
                }
                in_word = true;
            } else if (!is_word_byte(c)) {
                break;
            }
            *p = (char)toupper(c);
        }
        return;
    }

    for (char *p = text; *p != '\0'; ++p) {
        const unsigned char c = (unsigned char)*p;
        if (!isalpha(c)) {
            continue;
        }
        if (mode == CASE_MODE_CAP_FIRST_WORD) {
            *p = (char)toupper(c);
        } else if (mode == CASE_MODE_LOWER_FIRST_CHAR) {
            *p = (char)tolower(c);
        }
        return;
    }
}

static void formatted_note_text_append(Formatted_Text *formatted, Case_Mode *pending_case)
{
    if (formatted != NULL
        && pending_case != NULL
        && *pending_case != CASE_MODE_NORMAL
        && formatted->text_case == CASE_MODE_NORMAL
        && (formatted->text == NULL || formatted->text[0] == '\0')) {
        formatted->text_case = *pending_case;
        *pending_case = CASE_MODE_NORMAL;
    }
}

static bool formatted_append_char(Formatted_Text *formatted, char c, Case_Mode *pending_case)
{
    formatted_note_text_append(formatted, pending_case);
    return array_string_append_char(&formatted->text, c);
}

static bool formatted_append_range(
    Formatted_Text *formatted,
    const char *start,
    size_t length,
    Case_Mode *pending_case
)
{
    if (length > 0) {
        formatted_note_text_append(formatted, pending_case);
    }
    return array_string_append_range(&formatted->text, start, length);
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

static bool append_escaped_translation_byte(
    Formatted_Text *formatted,
    const char **cursor,
    Case_Mode *pending_case
)
{
    const char *p = *cursor;
    if (*p != '\\') {
        const bool ok = formatted_append_char(formatted, *p, pending_case);
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

    return formatted_append_char(formatted, c, pending_case);
}

static bool parse_attach_meta(
    Formatted_Text *formatted,
    const char *text,
    size_t length,
    Case_Mode *pending_case
)
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
    return formatted_append_range(formatted, text + start, end_index - start, pending_case);
}

static bool parse_ortho_attach_meta(
    Formatted_Text *formatted,
    const char *text,
    size_t length,
    Case_Mode *pending_case
)
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
    return formatted_append_range(formatted, suffix, suffix_length, pending_case);
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

static bool parse_case_mode_name(const char *name, size_t name_length, Case_Mode *out_mode)
{
    if (out_mode == NULL) {
        return false;
    }
    if (ascii_range_equals_ignore_case(name, name_length, "cap_first_word")) {
        *out_mode = CASE_MODE_CAP_FIRST_WORD;
        return true;
    }
    if (ascii_range_equals_ignore_case(name, name_length, "upper_first_word")) {
        *out_mode = CASE_MODE_UPPER_FIRST_WORD;
        return true;
    }
    if (ascii_range_equals_ignore_case(name, name_length, "lower_first_char")) {
        *out_mode = CASE_MODE_LOWER_FIRST_CHAR;
        return true;
    }
    return false;
}

static bool parse_case_meta(
    Formatted_Text *formatted,
    const char *meta_start,
    size_t meta_length,
    Case_Mode *pending_case
)
{
    if (meta_length == 1 && meta_start[0] == '>') {
        *pending_case = CASE_MODE_LOWER_FIRST_CHAR;
        return true;
    }
    if (meta_length == 1 && meta_start[0] == '<') {
        *pending_case = CASE_MODE_UPPER_FIRST_WORD;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '-' && meta_start[1] == '|') {
        *pending_case = CASE_MODE_CAP_FIRST_WORD;
        formatted->attach_next = true;
        return true;
    }
    if (meta_length > 6 && ascii_range_starts_with_ignore_case(meta_start, meta_length, ":case:")) {
        return parse_case_mode_name(meta_start + 6, meta_length - 6, pending_case);
    }
    if (meta_length > 12 && ascii_range_starts_with_ignore_case(meta_start, meta_length, ":retro_case:")) {
        return parse_case_mode_name(meta_start + 12, meta_length - 12, &formatted->retro_case);
    }
    if (meta_length == 3 && meta_start[0] == '*' && meta_start[1] == '-' && meta_start[2] == '|') {
        formatted->retro_case = CASE_MODE_CAP_FIRST_WORD;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '<') {
        formatted->retro_case = CASE_MODE_UPPER_FIRST_WORD;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '>') {
        formatted->retro_case = CASE_MODE_LOWER_FIRST_CHAR;
        return true;
    }
    return false;
}

static bool parse_punctuation_meta(
    Formatted_Text *formatted,
    const char *meta_start,
    size_t meta_length,
    Case_Mode *pending_case
)
{
    if (meta_length != 1 || strchr(".,:;?!", meta_start[0]) == NULL) {
        return false;
    }

    formatted->attach_prev = true;
    if (!formatted_append_char(formatted, meta_start[0], pending_case)) {
        return false;
    }
    if (meta_start[0] == '.' || meta_start[0] == '?' || meta_start[0] == '!') {
        formatted->next_case = CASE_MODE_CAP_FIRST_WORD;
    }
    return true;
}

static bool parse_key_combo_meta(Formatted_Text *formatted, const char *meta_start, size_t meta_length)
{
    if (meta_length < 2 || meta_start[0] != '#') {
        return false;
    }

    char *combo = copy_range(meta_start + 1, meta_length - 1);
    if (combo == NULL) {
        return false;
    }
    arrput(formatted->key_combos, combo);
    return true;
}

static bool parse_plover_command_meta(Formatted_Text *formatted, const char *meta_start, size_t meta_length)
{
    if (!ascii_range_starts_with_ignore_case(meta_start, meta_length, "PLOVER:")) {
        return false;
    }

    free(formatted->plover_command);
    formatted->plover_command = copy_range(meta_start + 7, meta_length - 7);
    return formatted->plover_command != NULL;
}

static bool parse_carry_capitalization_meta(
    Formatted_Text *formatted,
    const char *meta_start,
    size_t meta_length,
    Case_Mode *pending_case
)
{
    const char *separator = NULL;
    for (size_t i = 0; i + 1 < meta_length; ++i) {
        if (meta_start[i] == '~' && meta_start[i + 1] == '|') {
            separator = meta_start + i;
            break;
        }
    }
    if (separator == NULL) {
        return false;
    }

    bool begin = meta_length > 0 && meta_start[0] == '^';
    bool end = meta_length > 0 && meta_start[meta_length - 1] == '^';
    size_t start = (size_t)(separator - meta_start) + 2;
    size_t end_index = meta_length - (end ? 1 : 0);
    if (start > end_index) {
        start = end_index;
    }

    if (begin) {
        formatted->attach_prev = true;
    }
    if (end) {
        formatted->attach_next = true;
    }
    formatted->carry_case = true;
    return formatted_append_range(formatted, meta_start + start, end_index - start, pending_case);
}

static bool apply_translation_meta(
    Formatted_Text *formatted,
    const char *translation,
    const char *meta_start,
    size_t meta_length,
    bool *pending_attach_prev,
    Case_Mode *pending_case
)
{
    if (meta_length == 0) {
        formatted->cancel_formatting = true;
        *pending_case = CASE_MODE_NORMAL;
        return true;
    }

    if (parse_stitch_meta(formatted, meta_start, meta_length)
        || parse_stitch_retro_meta(formatted, meta_start, meta_length, ":stitch_last_word", false)
        || parse_stitch_retro_meta(formatted, meta_start, meta_length, ":stitch_phrase", true)) {
        return true;
    }

    if (parse_case_meta(formatted, meta_start, meta_length, pending_case)
        || parse_punctuation_meta(formatted, meta_start, meta_length, pending_case)
        || parse_key_combo_meta(formatted, meta_start, meta_length)
        || parse_plover_command_meta(formatted, meta_start, meta_length)
        || parse_carry_capitalization_meta(formatted, meta_start, meta_length, pending_case)) {
        return true;
    }

    if (meta_length == 1 && meta_start[0] == '#') {
        return true;
    }
    if (meta_length == 1 && meta_start[0] == '*') {
        formatted->plover_command = copy_cstring("retro_toggle_asterisk");
        return formatted->plover_command != NULL;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '!') {
        formatted->attach_prev = true;
        formatted->attach_next = true;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '?') {
        formatted->plover_command = copy_cstring("retro_insert_space");
        return formatted->plover_command != NULL;
    }
    if (meta_length >= 5 && ascii_range_starts_with_ignore_case(meta_start, meta_length, "MODE:")) {
        return true;
    }

    if (meta_length >= 5 && memcmp(meta_start, "glue:", 5) == 0) {
        formatted->glue = true;
        return formatted_append_range(formatted, meta_start + 5, meta_length - 5, pending_case);
    }
    if (meta_length >= 6 && memcmp(meta_start, ":glue:", 6) == 0) {
        formatted->glue = true;
        return formatted_append_range(formatted, meta_start + 6, meta_length - 6, pending_case);
    }
    if (meta_length >= 1 && meta_start[0] == '&') {
        formatted->glue = true;
        return formatted_append_range(formatted, meta_start + 1, meta_length - 1, pending_case);
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
        return formatted_append_range(formatted, translation, meta_length + 2, pending_case);
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
    if (parse_ortho_attach_meta(formatted, attach, attach_length, pending_case)) {
        return true;
    }
    return parse_attach_meta(formatted, attach, attach_length, pending_case);
}

static bool format_translation_text(const char *translation, Formatted_Text *out)
{
    if (translation == NULL || out == NULL) {
        return false;
    }
    if (strcmp(translation, "{^}^{^}") == 0) {
        out->attach_prev = true;
        out->attach_next = true;
        arrput(out->text, '\0');
        return true;
    }

    bool pending_attach_prev = false;
    Case_Mode pending_case = CASE_MODE_NORMAL;
    if (is_digit_string(translation)) {
        out->glue = true;
    }

    for (const char *p = translation; *p != '\0';) {
        if (*p == '\\') {
            if (pending_attach_prev) {
                out->attach_prev = true;
                pending_attach_prev = false;
            }
            if (!append_escaped_translation_byte(out, &p, &pending_case)) {
                return false;
            }
            continue;
        }

        if (*p != '{') {
            if (pending_attach_prev) {
                out->attach_prev = true;
                pending_attach_prev = false;
            }
            if (!formatted_append_char(out, *p++, &pending_case)) {
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
            if (!formatted_append_char(out, *p++, &pending_case)) {
                return false;
            }
            continue;
        }

        if (!apply_translation_meta(out, p, meta, (size_t)(end - meta), &pending_attach_prev, &pending_case)) {
            return false;
        }
        p = end + 1;
    }

    if (pending_attach_prev) {
        out->attach_prev = true;
        out->attach_next = true;
    }
    if (pending_case != CASE_MODE_NORMAL) {
        out->next_case = pending_case;
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
    for (size_t i = 0; i < arrlenu(formatted->key_combos); ++i) {
        free(formatted->key_combos[i]);
    }
    arrfree(formatted->key_combos);
    free(formatted->plover_command);
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

    const bool has_visible_text = emitted != NULL && emitted[0] != '\0';
    if (has_visible_text
        && !formatted->attach_next
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
    if (!append_string(&next.utf8, last->utf8)
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

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static bool path_matches_dictionary_selection(const char *path, const char *selection)
{
    if (path == NULL || selection == NULL) {
        return false;
    }

    const size_t path_length = strlen(path);
    const size_t selection_length = strlen(selection);
    if (selection_length == 0 || selection_length > path_length) {
        return false;
    }

    if (strcmp(path + path_length - selection_length, selection) == 0
        && (selection_length == path_length || path[path_length - selection_length - 1] == '/')) {
        return true;
    }

    const char *base = path_basename(path);
    const char *selection_base = path_basename(selection);
    if (strcmp(base, selection_base) == 0) {
        return true;
    }
    if (strncmp(base, "lapwing-", 8) == 0 && strcmp(base + 8, selection_base) == 0) {
        return true;
    }
    return false;
}

static char *copy_trimmed_range(const char *start, size_t length)
{
    while (length > 0 && isspace((unsigned char)*start)) {
        ++start;
        --length;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        --length;
    }
    return copy_range(start, length);
}

static bool toggle_dictionary_selection(Steno *steno, char toggle, const char *selection)
{
    size_t match = SIZE_MAX;
    size_t match_length = SIZE_MAX;
    for (size_t i = 0; i < arrlenu(steno->dictionary_paths); ++i) {
        if (!path_matches_dictionary_selection(steno->dictionary_paths[i], selection)) {
            continue;
        }
        const size_t length = strlen(steno->dictionary_paths[i]);
        if (length < match_length) {
            match = i;
            match_length = length;
        }
    }

    if (match == SIZE_MAX) {
        fprintf(stderr, "stoin: dictionary toggle could not find '%s'\n", selection);
        return true;
    }

    const bool old_enabled = steno->dictionary_enabled[match];
    bool new_enabled = old_enabled;
    if (toggle == '+') {
        new_enabled = true;
    } else if (toggle == '-') {
        new_enabled = false;
    } else if (toggle == '!') {
        new_enabled = !old_enabled;
    } else {
        fprintf(stderr, "stoin: invalid dictionary toggle '%c%s'\n", toggle, selection);
        return true;
    }

    if (new_enabled == old_enabled) {
        return true;
    }

    steno->dictionary_enabled[match] = new_enabled;
    if (!steno_reload_dictionary(steno)) {
        steno->dictionary_enabled[match] = old_enabled;
        (void)steno_reload_dictionary(steno);
        return false;
    }

    fprintf(stderr,
        "stoin: dictionary '%s' %s\n",
        steno->dictionary_paths[match],
        new_enabled ? "enabled" : "disabled");
    return true;
}

static bool execute_toggle_dict_command(Steno *steno, const char *selections)
{
    const char *p = selections;
    while (*p != '\0') {
        const char *end = strchr(p, ',');
        const size_t length = end == NULL ? strlen(p) : (size_t)(end - p);
        char *selection = copy_trimmed_range(p, length);
        if (selection == NULL) {
            return false;
        }
        if (selection[0] != '\0') {
            const char toggle = selection[0];
            const char *path = selection + 1;
            if (!toggle_dictionary_selection(steno, toggle, path)) {
                free(selection);
                return false;
            }
        }
        free(selection);
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return true;
}

static bool execute_plover_command(Steno *steno, const char *command)
{
    if (command == NULL || command[0] == '\0') {
        return true;
    }

    if (ascii_range_starts_with_ignore_case(command, strlen(command), "toggle_dict:")) {
        return execute_toggle_dict_command(steno, command + strlen("toggle_dict:"));
    }
    if (ascii_equals_ignore_case(command, "toggle")) {
        steno->enabled = !steno->enabled;
        reset_chord(steno);
        fprintf(stderr, "stoin: steno capture %s\n", steno->enabled ? "enabled" : "disabled");
        return true;
    }

    fprintf(stderr, "stoin: unsupported Plover command '%s'\n", command);
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

static bool apply_retro_case(Steno *steno, const Translation_Match *match, Case_Mode mode)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return true;
    }

    char *old_text = translation_range_text(steno->translations, translation_count - 1, 1);
    char *new_text = NULL;
    if (old_text == NULL || !append_string(&new_text, old_text)) {
        arrfree(old_text);
        arrfree(new_text);
        return false;
    }
    apply_case_mode_to_text(new_text, mode);

    Translation next = {0};
    if (!append_string(&next.utf8, new_text)
        || !translation_set_strokes(
            &next,
            steno->translations[translation_count - 1].strokes,
            arrlenu(steno->translations[translation_count - 1].strokes))
        || !translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        arrfree(old_text);
        arrfree(new_text);
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)) {
        arrfree(old_text);
        arrfree(new_text);
        translation_destroy(&next);
        return false;
    }

    arrput(next.replaced, steno->translations[translation_count - 1]);
    arrsetlen(steno->translations, translation_count - 1);
    arrput(steno->translations, next);

    arrfree(old_text);
    arrfree(new_text);
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

    if (formatted.retro_case != CASE_MODE_NORMAL) {
        const bool ok = apply_retro_case(steno, match, formatted.retro_case);
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
        const Case_Mode text_case = formatted.text_case != CASE_MODE_NORMAL
            ? formatted.text_case
            : steno->next_case;
        apply_case_mode_to_text(formatted.text, text_case);
        steno->next_case = formatted.next_case;
    } else if (formatted.next_case != CASE_MODE_NORMAL) {
        steno->next_case = formatted.next_case;
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
    if (!load_dictionary_from_paths(
            &next,
            steno->dictionary_paths,
            steno->dictionary_enabled,
            arrlenu(steno->dictionary_paths))) {
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
    steno->spacing = (Spacing_State) {
        .mode = SPACING_MODE_AFTER_WORD,
        .spacing_char = ' ',
    };
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_key_combination = config->send_key_combination;
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
            steno->dictionary_enabled,
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
