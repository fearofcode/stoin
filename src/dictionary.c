#include "dictionary.h"

#include "steno_stroke.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Dump_Entry {
    char stroke[64];
    const char *translation;
} Dump_Entry;

static const char *skip_json_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static bool parse_json_string(const char **cursor, char **out_string)
{
    const char *p = *cursor;
    if (*p != '"') {
        return false;
    }
    ++p;

    char *result = NULL;
    while (*p != '\0' && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            switch (c) {
            case '"': arrput(result, '"'); break;
            case '\\': arrput(result, '\\'); break;
            case '/': arrput(result, '/'); break;
            case 'b': arrput(result, '\b'); break;
            case 'f': arrput(result, '\f'); break;
            case 'n': arrput(result, '\n'); break;
            case 'r': arrput(result, '\r'); break;
            case 't': arrput(result, '\t'); break;
            case 'u':
                arrfree(result);
                return false;
            default:
                arrfree(result);
                return false;
            }
        } else {
            arrput(result, (char)c);
        }
    }

    if (*p != '"') {
        arrfree(result);
        return false;
    }
    ++p;

    arrput(result, '\0');
    *cursor = p;
    *out_string = result;
    return true;
}

bool dictionary_load(Dictionary *dictionary, const char *path)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read dictionary '%s'\n", path);
        return false;
    }

    const char *p = skip_json_ws(file);
    if (*p != '{') {
        fprintf(stderr, "stoin: dictionary '%s' is not a JSON object\n", path);
        free(file);
        return false;
    }
    ++p;

    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            break;
        }

        char *stroke = NULL;
        char *translation = NULL;
        if (!parse_json_string(&p, &stroke)) {
            break;
        }
        p = skip_json_ws(p);
        if (*p != ':') {
            arrfree(stroke);
            break;
        }
        ++p;
        p = skip_json_ws(p);
        if (!parse_json_string(&p, &translation)) {
            arrfree(stroke);
            break;
        }

        uint64_t bits = 0;
        if (stroke_string_to_bits(stroke, &bits)
            && hmgeti(dictionary->entries, bits) < 0) {
            Dictionary_Entry entry = {
                .key = bits,
                .value = copy_cstring(translation),
            };
            if (entry.value != NULL) {
                hmputs(dictionary->entries, entry);
            }
        }

        arrfree(stroke);
        arrfree(translation);

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            break;
        }
        break;
    }

    free(file);
    return hmlenu(dictionary->entries) > 0;
}

void dictionary_destroy(Dictionary *dictionary)
{
    if (dictionary == NULL) {
        return;
    }

    for (ptrdiff_t i = 0; i < hmlen(dictionary->entries); ++i) {
        free(dictionary->entries[i].value);
    }
    hmfree(dictionary->entries);
}

size_t dictionary_count(const Dictionary *dictionary)
{
    return dictionary == NULL ? 0 : hmlenu(dictionary->entries);
}

const char *dictionary_lookup_bits(const Dictionary *dictionary, uint64_t bits)
{
    if (dictionary == NULL) {
        return NULL;
    }

    Dictionary_Entry *entries = dictionary->entries;
    const Dictionary_Entry *entry = hmgetp_null(entries, bits);
    return entry == NULL ? NULL : entry->value;
}

bool dictionary_lookup_stroke(const Dictionary *dictionary, const char *stroke, const char **out_translation)
{
    uint64_t bits = 0;
    if (stroke == NULL || out_translation == NULL || !stroke_string_to_bits(stroke, &bits)) {
        return false;
    }

    const char *translation = dictionary_lookup_bits(dictionary, bits);
    if (translation == NULL) {
        return false;
    }

    *out_translation = translation;
    return true;
}

static int compare_dump_entries(const void *a, const void *b)
{
    const Dump_Entry *entry_a = a;
    const Dump_Entry *entry_b = b;
    return strcmp(entry_a->stroke, entry_b->stroke);
}

static bool write_json_string(FILE *file, const char *s)
{
    if (fputc('"', file) == EOF) {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", file) == EOF) return false;
            break;
        case '\\':
            if (fputs("\\\\", file) == EOF) return false;
            break;
        case '\b':
            if (fputs("\\b", file) == EOF) return false;
            break;
        case '\f':
            if (fputs("\\f", file) == EOF) return false;
            break;
        case '\n':
            if (fputs("\\n", file) == EOF) return false;
            break;
        case '\r':
            if (fputs("\\r", file) == EOF) return false;
            break;
        case '\t':
            if (fputs("\\t", file) == EOF) return false;
            break;
        default:
            if (*p < 0x20) {
                if (fprintf(file, "\\u%04x", *p) < 0) return false;
            } else if (fputc(*p, file) == EOF) {
                return false;
            }
            break;
        }
    }

    return fputc('"', file) != EOF;
}

bool dictionary_dump_json(const Dictionary *dictionary, const char *path)
{
    if (dictionary == NULL || path == NULL) {
        return false;
    }

    Dump_Entry *entries = NULL;
    for (ptrdiff_t i = 0; i < hmlen(dictionary->entries); ++i) {
        Dump_Entry entry = {
            .translation = dictionary->entries[i].value,
        };
        if (chord_bits_to_string(dictionary->entries[i].key, entry.stroke, sizeof(entry.stroke))) {
            arrput(entries, entry);
        }
    }

    qsort(entries, arrlenu(entries), sizeof(entries[0]), compare_dump_entries);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        arrfree(entries);
        return false;
    }

    bool ok = fputs("{\n", file) != EOF;
    for (size_t i = 0; ok && i < arrlenu(entries); ++i) {
        ok = fputs("  ", file) != EOF
            && write_json_string(file, entries[i].stroke)
            && fputs(": ", file) != EOF
            && write_json_string(file, entries[i].translation)
            && fputs(i + 1 == arrlenu(entries) ? "\n" : ",\n", file) != EOF;
    }
    ok = ok && fputs("}\n", file) != EOF;

    if (fclose(file) != 0) {
        ok = false;
    }
    arrfree(entries);
    return ok;
}
