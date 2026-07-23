#include "steno_internal.h"

#include "steno_stroke.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../third_party/stb_ds.h"
#include "../third_party/cjson/cJSON.h"

enum {
    BREVITY_BUFFER_BYTES = 1024,
    BREVITY_MAX_TRANSLATIONS = 5,
};

typedef struct Fixed_String_Buffer {
    char data[BREVITY_BUFFER_BYTES];
    size_t length;
    bool truncated;
} Fixed_String_Buffer;

static void fixed_string_buffer_reset(Fixed_String_Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }
    buffer->data[0] = '\0';
    buffer->length = 0;
    buffer->truncated = false;
}

static bool fixed_string_buffer_append_range(Fixed_String_Buffer *buffer, const char *start, size_t length)
{
    if (buffer == NULL || start == NULL) {
        return false;
    }
    if (length >= sizeof(buffer->data) || buffer->length > sizeof(buffer->data) - 1 - length) {
        buffer->truncated = true;
        return false;
    }
    memcpy(buffer->data + buffer->length, start, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool fixed_string_buffer_append_cstring(Fixed_String_Buffer *buffer, const char *s)
{
    if (s == NULL) {
        return false;
    }
    return fixed_string_buffer_append_range(buffer, s, strlen(s));
}

static const char *skip_leading_ascii_space(const char *s)
{
    if (s == NULL) {
        return "";
    }
    while (*s != '\0' && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

static bool append_translation_outline(
    Fixed_String_Buffer *outline,
    const Translation *translation,
    bool *has_stroke,
    size_t *stroke_count
)
{
    if (outline == NULL || translation == NULL || has_stroke == NULL || stroke_count == NULL) {
        return false;
    }

    for (size_t i = 0; i < arrlenu(translation->strokes); ++i) {
        char stroke[64] = {0};
        if (!chord_bits_to_string(translation->strokes[i], stroke, sizeof(stroke))) {
            return false;
        }
        if (*has_stroke && !fixed_string_buffer_append_range(outline, "/", 1)) {
            return false;
        }
        if (!fixed_string_buffer_append_cstring(outline, stroke)) {
            return false;
        }
        *has_stroke = true;
        ++*stroke_count;
    }
    return true;
}

static size_t outline_stroke_count(const char *outline)
{
    if (outline == NULL || outline[0] == '\0') {
        return 0;
    }

    size_t stroke_count = 1;
    for (const char *p = outline; *p != '\0'; ++p) {
        if (*p == '/') {
            ++stroke_count;
        }
    }
    return stroke_count;
}

static bool brevity_suggestions_enabled(const Steno *steno)
{
    return steno != NULL && (steno->print_suggestions || steno->suggestion_log_file != NULL);
}

static const char *suggestion_source_name(Phrase_Namespace namespace)
{
    switch (namespace) {
    case PHRASE_NAMESPACE_INITIAL_VERB:
        return "initial verb";
    case PHRASE_NAMESPACE_FINAL_VERB:
        return "final verb";
    case PHRASE_NAMESPACE_NONVERB:
        return "non verb";
    case PHRASE_NAMESPACE_NONE:
    default:
        return "dictionary";
    }
}

static const char *suggestion_source_id(Phrase_Namespace namespace)
{
    switch (namespace) {
    case PHRASE_NAMESPACE_INITIAL_VERB:
        return "initial_verb";
    case PHRASE_NAMESPACE_FINAL_VERB:
        return "final_verb";
    case PHRASE_NAMESPACE_NONVERB:
        return "non_verb";
    case PHRASE_NAMESPACE_NONE:
    default:
        return "dictionary";
    }
}

static void log_brevity_suggestion(
    Steno *steno,
    const char *suggested_outline,
    const char *typed_outline,
    const char *text,
    size_t typed_stroke_count,
    Phrase_Namespace source
)
{
    if (steno == NULL || steno->suggestion_log_file == NULL) {
        return;
    }

    const size_t suggested_stroke_count = outline_stroke_count(suggested_outline);
    const size_t saved_strokes = typed_stroke_count > suggested_stroke_count
        ? typed_stroke_count - suggested_stroke_count
        : 0;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }

    const time_t now = time(NULL);
    const bool ok = cJSON_AddNumberToObject(root, "unix_time", (double)now) != NULL
        && cJSON_AddStringToObject(root, "suggested_outline", suggested_outline) != NULL
        && cJSON_AddStringToObject(root, "typed_outline", typed_outline) != NULL
        && cJSON_AddStringToObject(root, "text", text) != NULL
        && cJSON_AddStringToObject(root, "source", suggestion_source_id(source)) != NULL
        && cJSON_AddNumberToObject(root, "typed_strokes", (double)typed_stroke_count) != NULL
        && cJSON_AddNumberToObject(root, "suggested_strokes", (double)suggested_stroke_count) != NULL
        && cJSON_AddNumberToObject(root, "saved_strokes", (double)saved_strokes) != NULL;
    char *json = ok ? cJSON_PrintUnformatted(root) : NULL;
    if (json != NULL) {
        fprintf(steno->suggestion_log_file, "%s\n", json);
        fflush(steno->suggestion_log_file);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

void steno_maybe_emit_brevity_suggestion(Steno *steno)
{
    if (!brevity_suggestions_enabled(steno)) {
        return;
    }

    const size_t translation_count = arrlenu(steno->translations);
    const size_t max_window = translation_count < BREVITY_MAX_TRANSLATIONS
        ? translation_count
        : BREVITY_MAX_TRANSLATIONS;
    for (size_t window = max_window; window > 0; --window) {
        const size_t start = translation_count - window;
        Fixed_String_Buffer text = {0};
        Fixed_String_Buffer typed_outline = {0};
        fixed_string_buffer_reset(&text);
        fixed_string_buffer_reset(&typed_outline);

        bool has_stroke = false;
        size_t typed_stroke_count = 0;
        bool ok = true;
        for (size_t i = start; i < translation_count; ++i) {
            const Translation *translation = &steno->translations[i];
            ok = fixed_string_buffer_append_cstring(&text, translation->utf8)
                && append_translation_outline(
                    &typed_outline,
                    translation,
                    &has_stroke,
                    &typed_stroke_count);
            if (!ok) {
                break;
            }
        }
        if (!ok || text.truncated || typed_outline.truncated || typed_stroke_count <= 1) {
            continue;
        }

        const char *candidate_text = skip_leading_ascii_space(text.data);
        if (candidate_text[0] == '\0') {
            continue;
        }

        char suggested_outline[BREVITY_BUFFER_BYTES] = {0};
        Phrase_Namespace source = PHRASE_NAMESPACE_NONE;
        const Phrase_Lookup_Result phrase_result = phrasing_find_translation_outline(
            steno->phrasing,
            candidate_text,
            typed_outline.data,
            typed_stroke_count - 1,
            &source,
            suggested_outline,
            sizeof(suggested_outline));
        if (phrase_result != PHRASE_LOOKUP_HIT
            && !dictionary_find_translation_outline(
                &steno->dictionary_stack.dictionary,
                candidate_text,
                typed_outline.data,
                typed_stroke_count - 1,
                suggested_outline,
                sizeof(suggested_outline))) {
            continue;
        }

        log_brevity_suggestion(
            steno,
            suggested_outline,
            typed_outline.data,
            candidate_text,
            typed_stroke_count,
            source);

        if (!steno->print_suggestions) {
            return;
        }
        FILE *file = steno->suggestions_file != NULL ? steno->suggestions_file : stdout;
        fprintf(file,
            "Suggestion [%s]: Use %s for \"%s\"\n",
            suggestion_source_name(source),
            suggested_outline,
            candidate_text);
        fflush(file);
        return;
    }
}
