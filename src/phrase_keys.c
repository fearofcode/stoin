#include "phrase_keys.h"

#include <stddef.h>

_Static_assert(
    PHRASE_KEY_BINDING_COUNT == PHRASE_NAMESPACE_NONVERB + 1,
    "phrase key bindings must cover every phrase namespace"
);

static bool phrase_namespace_is_bindable(Phrase_Namespace phrase_namespace)
{
    return phrase_namespace > PHRASE_NAMESPACE_NONE
        && phrase_namespace < PHRASE_KEY_BINDING_COUNT;
}

bool phrase_keys_init(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return false;
    }

    for (size_t i = 0; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        phrase_keys->bindings[i].down = NULL;
        phrase_keys->bindings[i].latched = NULL;
    }
    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        if (!phrase_keys->bindings[i].enabled) {
            continue;
        }
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

    for (size_t i = 0; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        platform_atomic_bool_destroy(binding->down);
        platform_atomic_bool_destroy(binding->latched);
        binding->down = NULL;
        binding->latched = NULL;
    }
}

bool phrase_keys_bind(
    Phrase_Keys *phrase_keys,
    Phrase_Namespace phrase_namespace,
    const char *name,
    uint16_t keycode
)
{
    if (phrase_keys == NULL || !phrase_namespace_is_bindable(phrase_namespace)) {
        return false;
    }

    Phrase_Key_Binding *binding = &phrase_keys->bindings[phrase_namespace];
    binding->name = name;
    binding->keycode = keycode;
    binding->enabled = true;
    platform_atomic_bool_store(binding->down, false);
    platform_atomic_bool_store(binding->latched, false);
    return true;
}

bool phrase_keys_have_distinct_keycodes(const Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return true;
    }

    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        const Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        if (!binding->enabled) {
            continue;
        }
        for (size_t j = i + 1; j < PHRASE_KEY_BINDING_COUNT; ++j) {
            const Phrase_Key_Binding *other = &phrase_keys->bindings[j];
            if (other->enabled && binding->keycode == other->keycode) {
                return false;
            }
        }
    }
    return true;
}

bool phrase_keys_any_enabled(const Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return false;
    }

    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        if (phrase_keys->bindings[i].enabled) {
            return true;
        }
    }
    return false;
}

const Phrase_Key_Binding *phrase_keys_get(
    const Phrase_Keys *phrase_keys,
    Phrase_Namespace phrase_namespace
)
{
    if (phrase_keys == NULL || !phrase_namespace_is_bindable(phrase_namespace)) {
        return NULL;
    }
    return &phrase_keys->bindings[phrase_namespace];
}

bool phrase_keys_handle_event(
    Phrase_Keys *phrase_keys,
    const Input_Event *event,
    Phrase_Namespace *out_namespace,
    bool *out_is_down
)
{
    if (phrase_keys == NULL || event == NULL) {
        return false;
    }

    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        if (!binding->enabled || binding->keycode != event->keycode) {
            continue;
        }

        if (out_namespace != NULL) {
            *out_namespace = (Phrase_Namespace)i;
        }
        if (out_is_down != NULL) {
            *out_is_down = event->is_down;
        }
        if (!event->is_repeat) {
            platform_atomic_bool_store(binding->down, event->is_down);
            if (event->is_down) {
                platform_atomic_bool_store(binding->latched, true);
            }
        }
        return true;
    }

    return false;
}

Phrase_Namespace phrase_keys_take_namespace(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return PHRASE_NAMESPACE_NONE;
    }

    bool active[PHRASE_KEY_BINDING_COUNT] = {false};
    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        if (!binding->enabled) {
            continue;
        }

        const bool down = platform_atomic_bool_load(binding->down);
        const bool latched = platform_atomic_bool_exchange(binding->latched, false);
        active[i] = down || latched;
    }

    return phrase_namespace_from_active_keys(
        active[PHRASE_NAMESPACE_INITIAL_VERB],
        active[PHRASE_NAMESPACE_FINAL_VERB],
        active[PHRASE_NAMESPACE_NONVERB]
    );
}

void phrase_keys_reset(Phrase_Keys *phrase_keys)
{
    if (phrase_keys == NULL) {
        return;
    }

    for (size_t i = PHRASE_NAMESPACE_INITIAL_VERB; i < PHRASE_KEY_BINDING_COUNT; ++i) {
        Phrase_Key_Binding *binding = &phrase_keys->bindings[i];
        platform_atomic_bool_store(binding->down, false);
        platform_atomic_bool_store(binding->latched, false);
    }
}
