#ifndef STENO_H
#define STENO_H

#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Steno Steno;

typedef bool (*Send_Text_Fn)(const char *utf8, void *userdata);
typedef bool (*Delete_Text_Fn)(const char *utf8, void *userdata);

typedef enum Spacing_Mode {
    SPACING_MODE_AFTER_WORD,
} Spacing_Mode;

typedef struct Spacing_State {
    Spacing_Mode mode;
    char spacing_char;
} Spacing_State;

typedef struct Steno_Config {
    const char *keymap_path;
    const char *dictionary_path;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    void *send_userdata;
    FILE *trace_file;
} Steno_Config;

Steno *steno_create(const Steno_Config *config);
void steno_destroy(Steno *steno);

bool steno_handle_event(Steno *steno, const Input_Event *event);
bool steno_handle_stroke_bits(Steno *steno, uint64_t bits);
void steno_set_session_active(Steno *steno, bool active);
size_t steno_key_binding_count(const Steno *steno);
size_t steno_dictionary_count(const Steno *steno);
size_t steno_translation_history_stroke_count(const Steno *steno);
bool steno_lookup_stroke(const Steno *steno, const char *stroke, const char **out_translation);
bool steno_dump_dictionary_json(const Steno *steno, const char *path);

#endif
