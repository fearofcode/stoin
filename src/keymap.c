#include "keymap.h"

#include "platform.h"
#include "steno_stroke.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

bool keymap_load(Keymap *keymap, const char *path)
{
    if (keymap == NULL || path == NULL) {
        return false;
    }

    char *file = read_entire_file(path, NULL);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read keymap '%s'\n", path);
        return false;
    }

    Key_Binding *bindings = NULL;
    char *cursor = file;
    int line_number = 1;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        while (isspace((unsigned char)*line)) {
            ++line;
        }
        if (*line == '\0' || (line[0] == '/' && line[1] == '/')) {
            ++line_number;
            continue;
        }

        char key_name[64] = {0};
        char steno_name[32] = {0};
        if (sscanf(line, "%63s %31s", key_name, steno_name) != 2) {
            fprintf(stderr, "stoin: invalid keymap line %d: %s\n", line_number, line);
            arrfree(bindings);
            free(file);
            return false;
        }

        uint16_t keycode = 0;
        uint64_t bits = 0;
        if (!platform_keycode_from_name(key_name, &keycode)) {
            fprintf(stderr, "stoin: unknown key name on keymap line %d: %s\n", line_number, key_name);
            arrfree(bindings);
            free(file);
            return false;
        }
        if (!stroke_string_to_bits(steno_name, &bits)) {
            fprintf(stderr, "stoin: invalid steno stroke on keymap line %d: %s\n", line_number, steno_name);
            arrfree(bindings);
            free(file);
            return false;
        }

        Key_Binding binding = {
            .keycode = keycode,
            .bits = bits,
        };
        arrput(bindings, binding);
        ++line_number;
    }

    free(file);
    if (arrlenu(bindings) == 0) {
        arrfree(bindings);
        return false;
    }

    keymap_destroy(keymap);
    keymap->bindings = bindings;
    return true;
}

void keymap_destroy(Keymap *keymap)
{
    if (keymap == NULL) {
        return;
    }
    arrfree(keymap->bindings);
    keymap->bindings = NULL;
}

const Key_Binding *keymap_find_binding(const Keymap *keymap, uint16_t keycode)
{
    if (keymap == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < arrlenu(keymap->bindings); ++i) {
        if (keymap->bindings[i].keycode == keycode) {
            return &keymap->bindings[i];
        }
    }
    return NULL;
}

size_t keymap_binding_count(const Keymap *keymap)
{
    return keymap == NULL ? 0 : arrlenu(keymap->bindings);
}
