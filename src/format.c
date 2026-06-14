#include "format.h"

#include "text_util.h"
#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

static bool is_word_byte(unsigned char c)
{
    return c >= 0x80 || isalnum(c) || c == '_';
}

void formatted_text_apply_case(char *text, Case_Mode mode)
{
    if (text == NULL || mode == CASE_MODE_NORMAL) {
        return;
    }

    if (mode == CASE_MODE_UPPER || mode == CASE_MODE_LOWER || mode == CASE_MODE_TITLE) {
        bool in_word = false;
        for (char *p = text; *p != '\0'; ++p) {
            const unsigned char c = (unsigned char)*p;
            if (!is_word_byte(c)) {
                in_word = false;
                continue;
            }

            if (mode == CASE_MODE_UPPER) {
                *p = (char)toupper(c);
            } else if (mode == CASE_MODE_LOWER) {
                *p = (char)tolower(c);
            } else {
                *p = in_word ? (char)tolower(c) : (char)toupper(c);
            }
            in_word = true;
        }
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
    return text_append_char(&formatted->text, c);
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
    return text_append_range(&formatted->text, start, length);
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
        && text_append_range(&formatted->text, args, word_length);
}

static bool parse_stitch_retro_meta(
    Formatted_Text *formatted,
    const char *meta_start,
    size_t meta_length,
    const char *command
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
    formatted->stitch_last_word = true;
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

static bool parse_mode_meta(Formatted_Text *formatted, const char *meta_start, size_t meta_length)
{
    if (!ascii_range_starts_with_ignore_case(meta_start, meta_length, "MODE:")) {
        return false;
    }

    free(formatted->mode_command);
    formatted->mode_command = copy_range(meta_start + 5, meta_length - 5);
    return formatted->mode_command != NULL;
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
        || parse_stitch_retro_meta(formatted, meta_start, meta_length, ":stitch_last_word")) {
        return true;
    }

    if (parse_case_meta(formatted, meta_start, meta_length, pending_case)
        || parse_punctuation_meta(formatted, meta_start, meta_length, pending_case)
        || parse_key_combo_meta(formatted, meta_start, meta_length)
        || parse_plover_command_meta(formatted, meta_start, meta_length)
        || parse_mode_meta(formatted, meta_start, meta_length)
        || parse_carry_capitalization_meta(formatted, meta_start, meta_length, pending_case)) {
        return true;
    }

    if (meta_length == 1 && meta_start[0] == '#') {
        return true;
    }
    if (meta_length == 1 && meta_start[0] == '*') {
        formatted->retro_command = RETRO_COMMAND_TOGGLE_ASTERISK;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '!') {
        formatted->retro_command = RETRO_COMMAND_DELETE_SPACE;
        return true;
    }
    if (meta_length == 2 && meta_start[0] == '*' && meta_start[1] == '?') {
        formatted->retro_command = RETRO_COMMAND_INSERT_SPACE;
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

bool format_translation_text(const char *translation, Formatted_Text *out)
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

void formatted_text_destroy(Formatted_Text *formatted)
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
    free(formatted->mode_command);
    memset(formatted, 0, sizeof(*formatted));
}
