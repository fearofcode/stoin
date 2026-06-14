#include "translation_history.h"

#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

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

void translation_destroy(Translation *translation)
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

bool translation_set_strokes(Translation *translation, const uint64_t *strokes, size_t stroke_count)
{
    if (translation == NULL || strokes == NULL || stroke_count == 0) {
        return false;
    }

    for (size_t i = 0; i < stroke_count; ++i) {
        arrput(translation->strokes, strokes[i]);
    }
    return true;
}

char *translation_range_text(const Translation *translations, size_t start, size_t count)
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

char *translation_replaced_text(const Translation *translation)
{
    if (translation == NULL) {
        return NULL;
    }
    return translation_range_text(translation->replaced, 0, arrlenu(translation->replaced));
}

char *translation_range_source_text(const Translation *translations, size_t start, size_t count)
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

char *translation_source_text(const Translation *translation)
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

size_t translation_history_stroke_count(const Translation *translations)
{
    size_t stroke_count = 0;
    for (size_t i = 0; i < arrlenu(translations); ++i) {
        stroke_count += arrlenu(translations[i].strokes);
    }
    return stroke_count;
}
