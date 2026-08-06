#ifndef PHRASE_KEYS_H
#define PHRASE_KEYS_H

#include "phrasing.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Phrase_Key_Binding {
    const char *name;
    uint16_t keycode;
    Platform_Atomic_Bool *down;
    Platform_Atomic_Bool *latched;
} Phrase_Key_Binding;

typedef struct Phrase_Keys {
    Phrase_Key_Binding *bindings;
} Phrase_Keys;

bool phrase_keys_init(Phrase_Keys *phrase_keys);
void phrase_keys_destroy(Phrase_Keys *phrase_keys);
bool phrase_keys_bind(
    Phrase_Keys *phrase_keys,
    const char *name,
    uint16_t keycode
);
bool phrase_keys_have_distinct_keycodes(const Phrase_Keys *phrase_keys);
bool phrase_keys_any_enabled(const Phrase_Keys *phrase_keys);
size_t phrase_keys_count(const Phrase_Keys *phrase_keys);
const Phrase_Key_Binding *phrase_keys_get(
    const Phrase_Keys *phrase_keys,
    size_t index
);
bool phrase_keys_handle_event(
    Phrase_Keys *phrase_keys,
    const Input_Event *event,
    bool *out_active
);
bool phrase_keys_take_active(Phrase_Keys *phrase_keys);
void phrase_keys_reset(Phrase_Keys *phrase_keys);

#endif
