#include "dictionary.h"

#include "steno_stroke.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Dump_Entry {
    const char *stroke;
    const char *translation;
} Dump_Entry;

enum {
    DICTIONARY_MAX_OUTLINE_BYTES = 4096,
};

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

static bool append_range(char *out, size_t out_size, size_t *index, const char *start, size_t length)
{
    if (*index + length >= out_size) {
        return false;
    }

    memcpy(out + *index, start, length);
    *index += length;
    out[*index] = '\0';
    return true;
}

static bool append_cstring(char *out, size_t out_size, size_t *index, const char *s)
{
    return append_range(out, out_size, index, s, strlen(s));
}

static bool strokes_to_outline_key(const uint64_t *strokes, size_t stroke_count, char *out, size_t out_size)
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
        if (i != 0 && !append_range(out, out_size, &index, "/", 1)) {
            return false;
        }
        if (!append_cstring(out, out_size, &index, stroke)) {
            return false;
        }
    }

    return true;
}

static bool outline_to_canonical_key(
    const char *outline,
    char *out,
    size_t out_size,
    size_t *out_stroke_count
)
{
    if (outline == NULL || out == NULL || out_size == 0) {
        return false;
    }

    size_t index = 0;
    size_t stroke_count = 0;
    out[0] = '\0';

    const char *segment = outline;
    while (*segment != '\0') {
        const char *end = strchr(segment, '/');
        const size_t length = end == NULL ? strlen(segment) : (size_t)(end - segment);
        if (length == 0 || length >= 64) {
            return false;
        }

        char stroke[64] = {0};
        memcpy(stroke, segment, length);

        uint64_t bits = 0;
        char canonical_stroke[64] = {0};
        if (!stroke_string_to_bits(stroke, &bits)
            || !chord_bits_to_string(bits, canonical_stroke, sizeof(canonical_stroke))) {
            return false;
        }

        if (stroke_count != 0 && !append_range(out, out_size, &index, "/", 1)) {
            return false;
        }
        if (!append_cstring(out, out_size, &index, canonical_stroke)) {
            return false;
        }
        ++stroke_count;

        if (end == NULL) {
            break;
        }
        segment = end + 1;
    }

    if (stroke_count == 0) {
        return false;
    }
    if (out_stroke_count != NULL) {
        *out_stroke_count = stroke_count;
    }
    return true;
}

static bool dictionary_put(Dictionary *dictionary, const char *canonical, size_t stroke_count, const char *translation)
{
    char *value = copy_cstring(translation);
    if (value == NULL) {
        return false;
    }

    const ptrdiff_t index = shgeti(dictionary->entries, canonical);
    if (index >= 0) {
        free(dictionary->entries[index].value);
        dictionary->entries[index].value = value;
    } else {
        shput(dictionary->entries, canonical, value);
    }

    if (stroke_count > dictionary->longest_key) {
        dictionary->longest_key = stroke_count;
    }
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

    if (dictionary->entries == NULL) {
        sh_new_strdup(dictionary->entries);
    }

    const char *p = skip_json_ws(file);
    if (*p != '{') {
        fprintf(stderr, "stoin: dictionary '%s' is not a JSON object\n", path);
        free(file);
        return false;
    }
    ++p;

    bool parsed_ok = false;
    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            parsed_ok = true;
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

        char canonical[DICTIONARY_MAX_OUTLINE_BYTES] = {0};
        size_t stroke_count = 0;
        if (outline_to_canonical_key(stroke, canonical, sizeof(canonical), &stroke_count)
            && !dictionary_put(dictionary, canonical, stroke_count, translation)) {
            arrfree(stroke);
            arrfree(translation);
            free(file);
            return false;
        }

        arrfree(stroke);
        arrfree(translation);

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            parsed_ok = true;
            break;
        }
        break;
    }

    free(file);
    if (!parsed_ok) {
        fprintf(stderr, "stoin: dictionary '%s' could not be parsed\n", path);
        return false;
    }

    if (hmlenu(dictionary->entries) == 0) {
        fprintf(stderr, "stoin: warning: dictionary '%s' is empty; untranslated chords will emit raw steno\n", path);
    }

    return true;
}

bool dictionary_load_many(Dictionary *dictionary, const char *const *paths, size_t path_count)
{
    if (dictionary == NULL || paths == NULL || path_count == 0) {
        return false;
    }

    for (size_t i = 0; i < path_count; ++i) {
        if (paths[i] == NULL || !dictionary_load(dictionary, paths[i])) {
            return false;
        }
    }
    return true;
}

void dictionary_destroy(Dictionary *dictionary)
{
    if (dictionary == NULL) {
        return;
    }

    for (ptrdiff_t i = 0; i < shlen(dictionary->entries); ++i) {
        free(dictionary->entries[i].value);
    }
    shfree(dictionary->entries);
    dictionary->entries = NULL;
    dictionary->longest_key = 0;
}

size_t dictionary_count(const Dictionary *dictionary)
{
    return dictionary == NULL ? 0 : shlenu(dictionary->entries);
}

size_t dictionary_longest_key(const Dictionary *dictionary)
{
    return dictionary == NULL ? 0 : dictionary->longest_key;
}

const char *dictionary_lookup_bits(const Dictionary *dictionary, uint64_t bits)
{
    return dictionary_lookup_strokes(dictionary, &bits, 1);
}

const char *dictionary_lookup_strokes(const Dictionary *dictionary, const uint64_t *strokes, size_t stroke_count)
{
    if (dictionary == NULL) {
        return NULL;
    }

    char canonical[DICTIONARY_MAX_OUTLINE_BYTES] = {0};
    if (!strokes_to_outline_key(strokes, stroke_count, canonical, sizeof(canonical))) {
        return NULL;
    }

    Dictionary_Entry *entries = dictionary->entries;
    const Dictionary_Entry *entry = shgetp_null(entries, canonical);
    return entry == NULL ? NULL : entry->value;
}

bool dictionary_lookup_stroke(const Dictionary *dictionary, const char *stroke, const char **out_translation)
{
    if (dictionary == NULL || stroke == NULL || out_translation == NULL) {
        return false;
    }

    char canonical[DICTIONARY_MAX_OUTLINE_BYTES] = {0};
    if (!outline_to_canonical_key(stroke, canonical, sizeof(canonical), NULL)) {
        return false;
    }

    Dictionary_Entry *entries = dictionary->entries;
    const Dictionary_Entry *entry = shgetp_null(entries, canonical);
    const char *translation = entry == NULL ? NULL : entry->value;
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
    for (ptrdiff_t i = 0; i < shlen(dictionary->entries); ++i) {
        Dump_Entry entry = {
            .stroke = dictionary->entries[i].key,
            .translation = dictionary->entries[i].value,
        };
        arrput(entries, entry);
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
