#ifndef STENO_INTERNAL_H
#define STENO_INTERNAL_H

#include "steno.h"

#include "dictionary_stack.h"
#include "format.h"
#include "keymap.h"
#include "orthography.h"
#include "phrasing.h"
#include "translation_history.h"
#include "translation_match.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Steno {
    Keymap keymap;
    Translation *translations;
    Dictionary_Stack dictionary_stack;
    Orthography orthography;
    Phrasing *phrasing;
    char *phrasing_path;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    Platform_File_Stamp phrasing_stamp;
    size_t strokes_since_compaction;
    Steno_Phrase_Mode chord_phrase_mode;
    bool phrase_namespace_enabled;
    Steno_Phrase_Mode phrase_mode;
    bool enabled;
    bool session_active;
    bool toggle_esc_down;
    bool control_down;
    bool option_down;
    bool command_down;
    Case_Mode case_mode;
    Case_Mode next_case;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    Send_Key_Combination_Fn send_key_combination;
    void *send_userdata;
    FILE *trace_file;
    FILE *suggestions_file;
    FILE *suggestion_log_file;
    bool phrasing_stamp_valid;
    bool phrasing_reload_error_reported;
    bool print_suggestions;
};


bool steno_set_spacing(Steno *steno, const char *spacing);
bool steno_execute_command(Steno *steno, const char *command, const uint64_t *strokes, size_t stroke_count);
bool steno_apply_translation_match(Steno *steno, const Translation_Match *match);
bool steno_translate_chord_bits(Steno *steno, uint64_t bits);
bool steno_translate_stroke_input(Steno *steno, Stroke_Input stroke);
void steno_maybe_emit_brevity_suggestion(Steno *steno);

#endif
