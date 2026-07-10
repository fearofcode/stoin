#include "translation_history.h"

#include "text_util.h"

#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

void translation_destroy(Translation *translation)
{
    if (translation == NULL) {
        return;
    }

    arrfree(translation->strokes);
    arrfree(translation->utf8);
    arrfree(translation->split_prefix_text);
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
        if (!text_append_cstring(&text, translations[start + i].utf8)) {
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
        if (source == NULL || !text_append_cstring(&text, source)) {
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
    if (!text_append_cstring(&text, translation->utf8)) {
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

void translation_history_compact(Translation **translations, size_t keep_strokes)
{
    if (translations == NULL || *translations == NULL) {
        return;
    }

    const size_t translation_count = arrlenu(*translations);
    if (translation_count == 0) {
        return;
    }

    size_t retained_strokes = 0;
    size_t retained_translations = 0;
    for (size_t i = translation_count; i > 0; --i) {
        ++retained_translations;
        retained_strokes += arrlenu((*translations)[i - 1].strokes);
        if (retained_strokes >= keep_strokes) {
            break;
        }
    }

    const size_t dropped_translations = translation_count - retained_translations;
    if (dropped_translations == 0) {
        return;
    }

    for (size_t i = 0; i < dropped_translations; ++i) {
        translation_destroy(&(*translations)[i]);
    }
    memmove(
        *translations,
        *translations + dropped_translations,
        retained_translations * sizeof((*translations)[0])
    );
    arrsetlen(*translations, retained_translations);
}
