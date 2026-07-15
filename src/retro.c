#include "retro.h"

#include "steno_stroke.h"
#include "text_util.h"

#include <string.h>

#include "../third_party/stb_ds.h"

static const char *retro_spacing(const Retro_Context *retro)
{
    return retro != NULL && retro->spacing != NULL && retro->spacing->spacing != NULL
        ? retro->spacing->spacing
        : "";
}

static const char *text_without_leading_spacing(const Retro_Context *retro, const char *text)
{
    if (text == NULL) {
        return "";
    }

    const char *spacing = retro_spacing(retro);
    const size_t spacing_length = strlen(spacing);
    if (retro != NULL
        && retro->spacing != NULL
        && retro->spacing->mode == SPACING_MODE_BEFORE_WORD
        && spacing_length > 0
        && strncmp(text, spacing, spacing_length) == 0) {
        return text + spacing_length;
    }
    return text;
}

static bool retro_replace_output(Retro_Context *retro, const char *old_text, const char *new_text)
{
    return retro != NULL
        && retro->replace_output != NULL
        && retro->replace_output(retro->userdata, old_text, new_text);
}

bool retro_apply_case(
    Retro_Context *retro,
    const uint64_t *strokes,
    size_t stroke_count,
    Case_Mode mode
)
{
    if (retro == NULL || retro->translations == NULL) {
        return false;
    }

    Translation *translations = *retro->translations;
    const size_t translation_count = arrlenu(translations);
    if (translation_count == 0) {
        return true;
    }

    char *old_text = translation_range_text(translations, translation_count - 1, 1);
    char *new_text = NULL;
    if (old_text == NULL || !text_append_cstring(&new_text, old_text)) {
        arrfree(old_text);
        arrfree(new_text);
        return false;
    }
    formatted_text_apply_case(new_text, mode);

    Translation next = {0};
    if (!text_append_cstring(&next.utf8, new_text)
        || !translation_set_strokes(
            &next,
            translations[translation_count - 1].strokes,
            arrlenu(translations[translation_count - 1].strokes))
        || !translation_set_strokes(&next, strokes, stroke_count)) {
        arrfree(old_text);
        arrfree(new_text);
        translation_destroy(&next);
        return false;
    }

    if (!retro_replace_output(retro, old_text, next.utf8)) {
        arrfree(old_text);
        arrfree(new_text);
        translation_destroy(&next);
        return false;
    }

    arrput(next.replaced, translations[translation_count - 1]);
    arrsetlen(*retro->translations, translation_count - 1);
    arrput(*retro->translations, next);

    arrfree(old_text);
    arrfree(new_text);
    return true;
}

bool retro_apply_delete_space(Retro_Context *retro, const uint64_t *strokes, size_t stroke_count)
{
    if (retro == NULL || retro->translations == NULL) {
        return false;
    }

    Translation *translations = *retro->translations;
    const size_t translation_count = arrlenu(translations);
    if (translation_count < 2) {
        return true;
    }
    if (translations[translation_count - 1].retro_space_command) {
        return true;
    }

    const size_t replace_start = translation_count - 2;
    const Translation *first = &translations[replace_start];
    const Translation *second = &translations[replace_start + 1];
    const char *first_text = first->utf8 != NULL ? first->utf8 : "";
    const char *second_text = second->utf8 != NULL ? second->utf8 : "";
    const char *second_without_spacing = text_without_leading_spacing(retro, second_text);

    char *old_text = translation_range_text(translations, replace_start, 2);
    char *new_text = NULL;
    if (old_text == NULL
        || !text_append_cstring(&new_text, first_text)
        || !text_append_cstring(&new_text, second_without_spacing)) {
        arrfree(old_text);
        arrfree(new_text);
        return false;
    }

    Translation next = {
        .utf8 = new_text,
        .retro_space_command = true,
    };
    new_text = NULL;
    if (!translation_set_strokes(&next, strokes, stroke_count)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    if (!retro_replace_output(retro, old_text, next.utf8)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    arrput(next.replaced, translations[replace_start]);
    arrput(next.replaced, translations[replace_start + 1]);
    arrsetlen(*retro->translations, replace_start);
    arrput(*retro->translations, next);

    arrfree(old_text);
    return true;
}

bool retro_apply_insert_space(Retro_Context *retro, const uint64_t *strokes, size_t stroke_count)
{
    if (retro == NULL || retro->translations == NULL) {
        return false;
    }

    Translation *translations = *retro->translations;
    const size_t translation_count = arrlenu(translations);
    if (translation_count == 0) {
        return true;
    }

    const Translation *last = &translations[translation_count - 1];
    if (!last->retro_space_command || arrlenu(last->replaced) == 0) {
        return true;
    }

    char *old_text = translation_range_text(translations, translation_count - 1, 1);
    char *new_text = translation_replaced_text(last);
    if (old_text == NULL || new_text == NULL) {
        arrfree(old_text);
        arrfree(new_text);
        return false;
    }

    Translation next = {
        .utf8 = new_text,
    };
    new_text = NULL;
    if (!translation_set_strokes(&next, strokes, stroke_count)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    if (!retro_replace_output(retro, old_text, next.utf8)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    arrput(next.replaced, translations[translation_count - 1]);
    arrsetlen(*retro->translations, translation_count - 1);
    arrput(*retro->translations, next);

    arrfree(old_text);
    return true;
}

bool retro_apply_toggle_asterisk(Retro_Context *retro)
{
    if (retro == NULL
        || retro->translations == NULL
        || retro->undo_last_translation == NULL
        || retro->translate_bits == NULL) {
        return false;
    }

    Translation *translations = *retro->translations;
    const size_t translation_count = arrlenu(translations);
    if (translation_count == 0) {
        return true;
    }

    const Translation *last = &translations[translation_count - 1];
    const size_t stroke_count = arrlenu(last->strokes);
    if (stroke_count == 0) {
        return true;
    }

    const uint64_t toggled_bits = last->strokes[stroke_count - 1] ^ steno_bit(STENO_STAR);
    return retro->undo_last_translation(retro->userdata)
        && retro->translate_bits(retro->userdata, toggled_bits);
}
