#include "platform.h"

#include <stdlib.h>
#include <windows.h>

struct Platform_Atomic_Bool {
    volatile LONG value;
};

Platform_Atomic_Bool *platform_atomic_bool_create(bool initial_value)
{
    Platform_Atomic_Bool *value = malloc(sizeof(*value));
    if (value == NULL) {
        return NULL;
    }
    value->value = initial_value ? 1 : 0;
    return value;
}

void platform_atomic_bool_destroy(Platform_Atomic_Bool *value)
{
    free(value);
}

void platform_atomic_bool_store(Platform_Atomic_Bool *value, bool new_value)
{
    if (value != NULL) {
        InterlockedExchange(&value->value, new_value ? 1 : 0);
    }
}

bool platform_atomic_bool_load(Platform_Atomic_Bool *value)
{
    return value != NULL && InterlockedCompareExchange(&value->value, 0, 0) != 0;
}

bool platform_atomic_bool_exchange(Platform_Atomic_Bool *value, bool new_value)
{
    return value != NULL
        && InterlockedExchange(&value->value, new_value ? 1 : 0) != 0;
}
