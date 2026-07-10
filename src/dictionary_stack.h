#ifndef DICTIONARY_STACK_H
#define DICTIONARY_STACK_H

#include "dictionary.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct Dictionary_Stack {
    char **paths;
    bool *enabled;
    Platform_File_Stamp *stamps;
    Dictionary dictionary;
    bool reload_error_reported;
} Dictionary_Stack;

bool dictionary_stack_set_paths(
    Dictionary_Stack *stack,
    const char *dictionary_path,
    const char *const *dictionary_paths,
    const bool *dictionary_enabled,
    size_t dictionary_path_count
);
bool dictionary_stack_load(Dictionary_Stack *stack);
bool dictionary_stack_reload(Dictionary_Stack *stack);
bool dictionary_stack_reload_if_changed(Dictionary_Stack *stack);
bool dictionary_stack_toggle(Dictionary_Stack *stack, const char *selections);
bool dictionary_stack_get_paths(const Dictionary_Stack *stack, const char *const **out_paths, size_t *out_path_count);
void dictionary_stack_destroy(Dictionary_Stack *stack);

#endif
