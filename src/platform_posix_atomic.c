#include "platform.h"

#include <stdatomic.h>
#include <stdlib.h>

struct Platform_Atomic_Bool {
    atomic_bool value;
};

Platform_Atomic_Bool *platform_atomic_bool_create(bool initial_value)
{
    Platform_Atomic_Bool *value = malloc(sizeof(*value));
    if (value == NULL) {
        return NULL;
    }
    atomic_init(&value->value, initial_value);
    return value;
}

void platform_atomic_bool_destroy(Platform_Atomic_Bool *value)
{
    free(value);
}

void platform_atomic_bool_store(Platform_Atomic_Bool *value, bool new_value)
{
    if (value != NULL) {
        atomic_store_explicit(&value->value, new_value, memory_order_relaxed);
    }
}

bool platform_atomic_bool_load(Platform_Atomic_Bool *value)
{
    return value != NULL && atomic_load_explicit(&value->value, memory_order_relaxed);
}

bool platform_atomic_bool_exchange(Platform_Atomic_Bool *value, bool new_value)
{
    return value != NULL && atomic_exchange_explicit(&value->value, new_value, memory_order_relaxed);
}
