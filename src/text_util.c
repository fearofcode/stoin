#include "text_util.h"

#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

bool text_append_char(char **out, char c)
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

bool text_append_range(char **out, const char *start, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!text_append_char(out, start[i])) {
            return false;
        }
    }
    return true;
}

bool text_append_cstring(char **out, const char *s)
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

bool text_prepend_range(char **out, const char *start, size_t length)
{
    if (out == NULL || start == NULL) {
        return false;
    }

    char *result = NULL;
    if (!text_append_range(&result, start, length)
        || (*out != NULL && !text_append_range(&result, *out, strlen(*out)))) {
        arrfree(result);
        return false;
    }

    arrfree(*out);
    *out = result;
    return true;
}

bool ascii_range_equals_ignore_case(const char *a, size_t a_length, const char *b)
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

bool ascii_range_starts_with_ignore_case(const char *s, size_t length, const char *prefix)
{
    if (s == NULL || prefix == NULL) {
        return false;
    }
    const size_t prefix_length = strlen(prefix);
    return length >= prefix_length && ascii_range_equals_ignore_case(s, prefix_length, prefix);
}

bool text_is_plain_multiword(const char *text)
{
    if (text == NULL || strchr(text, '{') != NULL || strchr(text, '}') != NULL) {
        return false;
    }

    bool saw_word = false;
    bool saw_separator = false;
    for (const char *p = text; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) {
            saw_separator = saw_word;
        } else {
            if (saw_separator) {
                return true;
            }
            saw_word = true;
        }
    }
    return false;
}

char *copy_trimmed_range(const char *start, size_t length)
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

size_t utf8_codepoint_length(const char *s, const char *end)
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

size_t common_utf8_prefix_bytes(const char *a, const char *b)
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
