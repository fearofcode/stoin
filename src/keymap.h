#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Key_Binding {
    uint16_t keycode;
    uint64_t bits;
} Key_Binding;

typedef struct Keymap {
    Key_Binding *bindings;
} Keymap;

bool keymap_load(Keymap *keymap, const char *path);
void keymap_destroy(Keymap *keymap);
const Key_Binding *keymap_find_binding(const Keymap *keymap, uint16_t keycode);
size_t keymap_binding_count(const Keymap *keymap);

#endif
