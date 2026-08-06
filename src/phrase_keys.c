#include "phrase_keys.h"

#include <stddef.h>

#include "../third_party/stb_ds.h"

bool phrase_keys_init(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return false;
    }

    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        binding->down = platform_atomic_bool_create(false);
        binding->latched = platform_atomic_bool_create(false);
        if (binding->down == NULL || binding->latched == NULL) {
            phrase_keys_destroy(phrase_keys);
            return false;
        }
    }
    return true;
}

void phrase_keys_destroy(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return;
    }

    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        platform_atomic_bool_destroy(binding->down);
        platform_atomic_bool_destroy(binding->latched);
        binding->down = NULL;
        binding->latched = NULL;
    }
    arrfree(phrase_keys->bindings);
}

bool phrase_keys_bind(
    Phrase_Keys *phrase_keys,
    const char *name,
    uint16_t keycode
)
{
    if (phrase_keys == NULL || name == NULL) {
        return false;
    }

    arrput(phrase_keys->bindings, ((Phrase_Key_Binding) {
        .name = name,
        .keycode = keycode,
    }));
    return true;
}

bool phrase_keys_have_distinct_keycodes(const Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return true;
    }

    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        for (size_t j = i + 1; j < arrlenu(phrase_keys->bindings); ++j) {
            if (phrase_keys->bindings[i].keycode == phrase_keys->bindings[j].keycode) {
                return false;
            }
        }
    }
    return true;
}

bool phrase_keys_any_enabled(const Phrase_Keys *phrase_keys)
{
    return phrase_keys_count(phrase_keys) > 0;
}

size_t phrase_keys_count(const Phrase_Keys *phrase_keys)
{
    return phrase_keys == NULL ? 0 : arrlenu(phrase_keys->bindings);
}

const Phrase_Key_Binding *phrase_keys_get(
    const Phrase_Keys *phrase_keys,
    size_t index
)
{
    if (phrase_keys == NULL || index >= arrlenu(phrase_keys->bindings)) {
        return NULL;
    }
    return &phrase_keys->bindings[index];
}

static bool phrase_keys_any_down(const Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return false;
    }
    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        if (platform_atomic_bool_load(phrase_keys->bindings[i].down)) {
            return true;
        }
    }
    return false;
}

bool phrase_keys_handle_event(
    Phrase_Keys *phrase_keys,
    const Input_Event *event,
    bool *out_active
)
{
    if (phrase_keys == NULL || event == NULL) {
        return false;
    }

    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        if (binding->keycode != event->keycode) {
            continue;
        }

        if (!event->is_repeat) {
            platform_atomic_bool_store(binding->down, event->is_down);
            if (event->is_down) {
                platform_atomic_bool_store(binding->latched, true);
            }
        }
        if (out_active != NULL) {
            *out_active = phrase_keys_any_down(phrase_keys);
        }
        return true;
    }

    return false;
}

bool phrase_keys_take_active(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return false;
    }

    bool active = false;
    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        const bool down = platform_atomic_bool_load(binding->down);
        const bool latched = platform_atomic_bool_exchange(binding->latched, false);
        active = active || down || latched;
    }
    return active;
}

void phrase_keys_reset(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return;
    }

    for (size_t i = 0; i < arrlenu(phrase_keys->bindings); ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        platform_atomic_bool_store(binding->down, false);
        platform_atomic_bool_store(binding->latched, false);
    }
}
