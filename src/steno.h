#ifndef STOIN_STENO_H
#define STOIN_STENO_H

#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Stoin_Steno Stoin_Steno;

typedef bool (*Stoin_Steno_Send_Text_Fn)(const char *utf8, void *userdata);

typedef struct Stoin_Steno_Config {
    const char *keymap_path;
    const char *dictionary_path;
    Stoin_Steno_Send_Text_Fn send_text;
    void *send_userdata;
} Stoin_Steno_Config;

Stoin_Steno *stoin_steno_create(const Stoin_Steno_Config *config);
void stoin_steno_destroy(Stoin_Steno *steno);

bool stoin_steno_handle_event(Stoin_Steno *steno, const Stoin_Input_Event *event);
size_t stoin_steno_key_binding_count(const Stoin_Steno *steno);
size_t stoin_steno_dictionary_count(const Stoin_Steno *steno);
bool stoin_steno_lookup_stroke(const Stoin_Steno *steno, const char *stroke, const char **out_translation);
bool stoin_steno_dump_dictionary_json(const Stoin_Steno *steno, const char *path);
bool stoin_steno_run_self_test(const Stoin_Steno_Config *config);

#endif
