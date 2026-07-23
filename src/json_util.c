#include "json_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool append_char(char **value, size_t *length, size_t *capacity, char c)
{
    if (*length + 1 >= *capacity) {
        const size_t next_capacity = *capacity == 0 ? 16 : *capacity * 2;
        char *next = realloc(*value, next_capacity);
        if (next == NULL) {
            free(*value);
            *value = NULL;
            *length = 0;
            *capacity = 0;
            return false;
        }
        *value = next;
        *capacity = next_capacity;
    }
    (*value)[(*length)++] = c;
    return true;
}

const char *json_find_object(const char *json, const char *name, const char **out_end)
{
    if (json == NULL || name == NULL) {
        return NULL;
    }

    char pattern[64] = {0};
    snprintf(pattern, sizeof(pattern), "\"%s\"", name);

    const char *name_pos = strstr(json, pattern);
    if (name_pos == NULL) {
        return NULL;
    }

    const char *start = strchr(name_pos, '{');
    if (start == NULL) {
        return NULL;
    }

    const char *p = start;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    while (*p != '\0') {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (*p == '\\') {
                escaped = true;
            } else if (*p == '"') {
                in_string = false;
            }
        } else if (*p == '"') {
            in_string = true;
        } else if (*p == '{') {
            ++depth;
        } else if (*p == '}') {
            --depth;
            if (depth == 0) {
                if (out_end != NULL) {
                    *out_end = p;
                }
                return start;
            }
            if (depth < 0) {
                return NULL;
            }
        }
        ++p;
    }
    return NULL;
}

bool json_parse_uint_field(const char *start, const char *end, const char *name, uint32_t *out_value)
{
    if (start == NULL || end == NULL || name == NULL || out_value == NULL) {
        return false;
    }

    char pattern[48] = {0};
    snprintf(pattern, sizeof(pattern), "\"%s\"", name);

    const char *field = strstr(start, pattern);
    if (field == NULL || field >= end) {
        return false;
    }

    const char *colon = strchr(field, ':');
    if (colon == NULL || colon >= end) {
        return false;
    }

    char *parse_end = NULL;
    const unsigned long parsed = strtoul(colon + 1, &parse_end, 0);
    if (parse_end == colon + 1 || parse_end > end || parsed > UINT32_MAX) {
        return false;
    }

    *out_value = (uint32_t)parsed;
    return true;
}

static bool parse_string_value(const char **cursor, const char *end, char **out_value)
{
    const char *p = *cursor;
    while (p < end && isspace((unsigned char)*p)) {
        ++p;
    }
    if (p >= end || *p != '"') {
        return false;
    }
    ++p;

    char *value = NULL;
    size_t length = 0;
    size_t capacity = 0;
    while (p < end && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            if (p >= end) {
                free(value);
                return false;
            }
            c = (unsigned char)*p++;
            switch (c) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            default:
                free(value);
                return false;
            }
        }
        if (!append_char(&value, &length, &capacity, (char)c)) {
            return false;
        }
    }

    if (p >= end || *p != '"') {
        free(value);
        return false;
    }
    if (!append_char(&value, &length, &capacity, '\0')) {
        return false;
    }
    *cursor = p + 1;
    *out_value = value;
    return true;
}

bool json_parse_string_field(const char *start, const char *end, const char *name, char **out_value)
{
    if (start == NULL || end == NULL || name == NULL || out_value == NULL) {
        return false;
    }

    char pattern[48] = {0};
    snprintf(pattern, sizeof(pattern), "\"%s\"", name);

    const char *field = strstr(start, pattern);
    if (field == NULL || field >= end) {
        return false;
    }

    const char *colon = strchr(field, ':');
    if (colon == NULL || colon >= end) {
        return false;
    }
    ++colon;

    return parse_string_value(&colon, end, out_value);
}

void json_write_string(FILE *file, const char *value)
{
    fputc('"', file);
    for (const char *p = value != NULL ? value : ""; *p != '\0'; ++p) {
        switch (*p) {
        case '"': fputs("\\\"", file); break;
        case '\\': fputs("\\\\", file); break;
        case '\b': fputs("\\b", file); break;
        case '\f': fputs("\\f", file); break;
        case '\n': fputs("\\n", file); break;
        case '\r': fputs("\\r", file); break;
        case '\t': fputs("\\t", file); break;
        default: fputc(*p, file); break;
        }
    }
    fputc('"', file);
}
