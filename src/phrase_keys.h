#ifndef PHRASE_KEYS_H
#define PHRASE_KEYS_H

#include "phrasing.h"
#include "platform.h"

#include <stdbool.h>
#include <stdint.h>

#define PHRASE_KEY_BINDING_COUNT 4

typedef struct Phrase_Key_Binding {
    const char *name;
    uint16_t keycode;
    bool enabled;
    Platform_Atomic_Bool *down;
    Platform_Atomic_Bool *latched;
} Phrase_Key_Binding;

typedef struct Phrase_Keys {
    Phrase_Key_Binding bindings[PHRASE_KEY_BINDING_COUNT];
} Phrase_Keys;

bool phrase_keys_init(Phrase_Keys *phrase_keys);
void phrase_keys_destroy(Phrase_Keys *phrase_keys);
bool phrase_keys_bind(
    Phrase_Keys *phrase_keys,
    Phrase_Namespace phrase_namespace,
    const char *name,
    uint16_t keycode
);
bool phrase_keys_have_distinct_keycodes(const Phrase_Keys *phrase_keys);
bool phrase_keys_any_enabled(const Phrase_Keys *phrase_keys);
const Phrase_Key_Binding *phrase_keys_get(
    const Phrase_Keys *phrase_keys,
    Phrase_Namespace phrase_namespace
);
bool phrase_keys_handle_event(
    Phrase_Keys *phrase_keys,
    const Input_Event *event,
    Phrase_Namespace *out_namespace,
    bool *out_is_down
);
Phrase_Namespace phrase_keys_take_namespace(Phrase_Keys *phrase_keys);
void phrase_keys_reset(Phrase_Keys *phrase_keys);

#endif
