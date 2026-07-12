#ifndef RETRO_H
#define RETRO_H

#include "format.h"
#include "steno.h"
#include "translation_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*Retro_Replace_Output_Fn)(void *userdata, const char *old_text, const char *new_text);
typedef bool (*Retro_Undo_Last_Translation_Fn)(void *userdata);
typedef bool (*Retro_Translate_Bits_Fn)(void *userdata, uint64_t bits, Translation_Source source);

typedef struct Retro_Context {
    Translation **translations;
    const Spacing_State *spacing;
    Retro_Replace_Output_Fn replace_output;
    Retro_Undo_Last_Translation_Fn undo_last_translation;
    Retro_Translate_Bits_Fn translate_bits;
    Translation_Source source;
    void *userdata;
} Retro_Context;

bool retro_apply_case(
    Retro_Context *retro,
    const uint64_t *strokes,
    size_t stroke_count,
    Case_Mode mode
);
bool retro_apply_delete_space(Retro_Context *retro, const uint64_t *strokes, size_t stroke_count);
bool retro_apply_insert_space(Retro_Context *retro, const uint64_t *strokes, size_t stroke_count);
bool retro_apply_toggle_asterisk(Retro_Context *retro);

#endif
