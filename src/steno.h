#ifndef STENO_H
#define STENO_H

#include "platform.h"
#include "phrasing.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Steno Steno;

typedef bool (*Send_Text_Fn)(const char *utf8, void *userdata);
typedef bool (*Delete_Text_Fn)(const char *utf8, void *userdata);
typedef bool (*Send_Key_Combination_Fn)(const char *combo, void *userdata);

typedef enum Spacing_Mode {
    SPACING_MODE_BEFORE_WORD,
} Spacing_Mode;

typedef struct Spacing_State {
    Spacing_Mode mode;
    char *spacing;
} Spacing_State;

typedef struct Stroke_Input {
    uint64_t bits;
    Phrase_Namespace phrase_namespace;
    uint64_t received_ns;
} Stroke_Input;

typedef struct Steno_Config {
    const char *keymap_path;
    const char *dictionary_path;
    const char *const *dictionary_paths;
    const bool *dictionary_enabled;
    size_t dictionary_path_count;
    const char *word_list_path;
    const char *phrasing_path;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    Send_Key_Combination_Fn send_key_combination;
    void *send_userdata;
    FILE *trace_file;
    FILE *suggestions_file;
    FILE *suggestion_log_file;
    bool print_suggestions;
} Steno_Config;

Steno *steno_create(const Steno_Config *config);
void steno_destroy(Steno *steno);

bool steno_handle_event(Steno *steno, const Input_Event *event);
bool steno_handle_stroke(Steno *steno, Stroke_Input stroke);
bool steno_handle_stroke_bits(Steno *steno, uint64_t bits);
void steno_set_phrase_namespace(Steno *steno, Phrase_Namespace namespace, bool is_down);
void steno_set_session_active(Steno *steno, bool active);
bool steno_reload_dictionary(Steno *steno);
bool steno_reload_dictionary_if_changed(Steno *steno);
bool steno_reload_phrasing(Steno *steno);
bool steno_reload_phrasing_if_changed(Steno *steno);
uint64_t steno_source_revision(const Steno *steno);
bool steno_get_dictionary_paths(const Steno *steno, const char *const **out_paths, size_t *out_path_count);
bool steno_get_phrasing_path(const Steno *steno, const char **out_path);
size_t steno_key_binding_count(const Steno *steno);
size_t steno_dictionary_count(const Steno *steno);
size_t steno_translation_history_stroke_count(const Steno *steno);
bool steno_lookup_stroke(Steno *steno, const char *stroke, const char **out_translation);
bool steno_dump_dictionary_json(const Steno *steno, const char *path);
bool steno_write_hint_index(Steno *steno, const char *path);

#endif
