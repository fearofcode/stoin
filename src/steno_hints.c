#include "steno_internal.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

typedef struct Hint_Value {
    char *outline;
    Phrase_Namespace source;
    size_t stroke_count;
} Hint_Value;

typedef struct Hint_Entry {
    char *key;
    Hint_Value value;
} Hint_Entry;

typedef struct Hint_Builder {
    Hint_Entry *entries;
    bool ok;
} Hint_Builder;

static size_t outline_stroke_count(const char *outline)
{
    if (outline == NULL || outline[0] == '\0') {
        return 0;
    }

    size_t count = 1;
    for (const char *p = outline; *p != '\0'; ++p) {
        if (*p == '/') {
            ++count;
        }
    }
    return count;
}

static bool translation_can_be_hinted(const char *translation)
{
    return translation != NULL
        && translation[0] != '\0'
        && strpbrk(translation, "{}=") == NULL;
}

static bool dictionary_hint_is_better(
    const char *outline,
    size_t stroke_count,
    const Hint_Value *current
)
{
    if (current == NULL || current->outline == NULL) {
        return true;
    }
    const size_t outline_length = strlen(outline);
    const size_t current_length = strlen(current->outline);
    return stroke_count < current->stroke_count
        || (stroke_count == current->stroke_count && outline_length < current_length)
        || (stroke_count == current->stroke_count
            && outline_length == current_length
            && strcmp(outline, current->outline) < 0);
}

static bool set_hint(
    Hint_Builder *builder,
    const char *text,
    const char *outline,
    Phrase_Namespace source,
    bool always_replace
)
{
    if (builder == NULL || text == NULL || outline == NULL) {
        return false;
    }

    const size_t stroke_count = outline_stroke_count(outline);
    if (stroke_count == 0) {
        return true;
    }

    const ptrdiff_t index = shgeti(builder->entries, text);
    if (index >= 0
        && !always_replace
        && !dictionary_hint_is_better(
            outline,
            stroke_count,
            &builder->entries[index].value)) {
        return true;
    }

    char *outline_copy = copy_cstring(outline);
    if (outline_copy == NULL) {
        return false;
    }
    const Hint_Value value = {
        .outline = outline_copy,
        .source = source,
        .stroke_count = stroke_count,
    };
    if (index >= 0) {
        free(builder->entries[index].value.outline);
        builder->entries[index].value = value;
    } else {
        shput(builder->entries, text, value);
    }
    return true;
}

static bool add_phrase_hint(
    const char *text,
    const char *outline,
    Phrase_Namespace namespace,
    void *userdata
)
{
    Hint_Builder *builder = userdata;
    if (builder == NULL || !builder->ok) {
        return false;
    }
    builder->ok = set_hint(builder, text, outline, namespace, true);
    return builder->ok;
}

static void destroy_hint_entries(Hint_Entry *entries)
{
    for (ptrdiff_t i = 0; i < shlen(entries); ++i) {
        free(entries[i].value.outline);
    }
    shfree(entries);
}

static bool write_json_string(FILE *file, const char *value)
{
    if (file == NULL || value == NULL || fputc('"', file) == EOF) {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", file) == EOF) {
                return false;
            }
            break;
        case '\\':
            if (fputs("\\\\", file) == EOF) {
                return false;
            }
            break;
        case '\b':
            if (fputs("\\b", file) == EOF) {
                return false;
            }
            break;
        case '\f':
            if (fputs("\\f", file) == EOF) {
                return false;
            }
            break;
        case '\n':
            if (fputs("\\n", file) == EOF) {
                return false;
            }
            break;
        case '\r':
            if (fputs("\\r", file) == EOF) {
                return false;
            }
            break;
        case '\t':
            if (fputs("\\t", file) == EOF) {
                return false;
            }
            break;
        default:
            if (*p < 0x20) {
                if (fprintf(file, "\\u%04x", (unsigned int)*p) < 0) {
                    return false;
                }
            } else if (fputc(*p, file) == EOF) {
                return false;
            }
            break;
        }
    }
    return fputc('"', file) != EOF;
}

static const char *hint_source_id(Phrase_Namespace source)
{
    switch (source) {
    case PHRASE_NAMESPACE_INITIAL_VERB:
        return "initial_verb";
    case PHRASE_NAMESPACE_FINAL_VERB:
        return "final_verb";
    case PHRASE_NAMESPACE_PASSIVE_FINAL_VERB:
        return "passive_final_verb";
    case PHRASE_NAMESPACE_NONVERB:
        return "non_verb";
    case PHRASE_NAMESPACE_NONE:
    default:
        return "dictionary";
    }
}

static bool write_hint_entries(FILE *file, const Hint_Entry *entries)
{
    if (fputs("{\"version\":1,\"hints\":{", file) == EOF) {
        return false;
    }
    for (ptrdiff_t i = 0; i < shlen(entries); ++i) {
        if ((i != 0 && fputc(',', file) == EOF)
            || !write_json_string(file, entries[i].key)
            || fputs(":{\"outline\":", file) == EOF
            || !write_json_string(file, entries[i].value.outline)
            || fputs(",\"source\":", file) == EOF
            || !write_json_string(file, hint_source_id(entries[i].value.source))
            || fputc('}', file) == EOF) {
            return false;
        }
    }
    return fputs("}}\n", file) != EOF;
}

bool steno_write_hint_index(Steno *steno, const char *path)
{
    if (steno == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    Hint_Builder builder = {.ok = true};
    sh_new_strdup(builder.entries);
    Dictionary *dictionary = &steno->dictionary_stack.dictionary;
    for (ptrdiff_t i = 0; i < shlen(dictionary->entries); ++i) {
        const char *outline = dictionary->entries[i].key;
        const char *translation = dictionary->entries[i].value;
        if (translation_can_be_hinted(translation)
            && !set_hint(
                &builder,
                translation,
                outline,
                PHRASE_NAMESPACE_NONE,
                false)) {
            builder.ok = false;
            break;
        }
    }

    if (builder.ok
        && steno->phrasing != NULL
        && !phrasing_for_each_suggestion(steno->phrasing, add_phrase_hint, &builder)) {
        builder.ok = false;
    }
    if (!builder.ok) {
        destroy_hint_entries(builder.entries);
        return false;
    }

    const size_t path_length = strlen(path);
    char *temporary_path = malloc(path_length + sizeof(".tmp"));
    if (temporary_path == NULL) {
        destroy_hint_entries(builder.entries);
        return false;
    }
    memcpy(temporary_path, path, path_length);
    memcpy(temporary_path + path_length, ".tmp", sizeof(".tmp"));

    FILE *file = fopen(temporary_path, "wb");
    bool ok = file != NULL && write_hint_entries(file, builder.entries);
    if (file != NULL && fclose(file) != 0) {
        ok = false;
    }
    if (ok) {
#if defined(_WIN32)
        (void)remove(path);
#endif
        ok = rename(temporary_path, path) == 0;
    }
    if (!ok) {
        (void)remove(temporary_path);
    }

    free(temporary_path);
    destroy_hint_entries(builder.entries);
    return ok;
}
