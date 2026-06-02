#include "steno.h"

#include "dictionary.h"
#include "steno_stroke.h"
#include "util.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Key_Binding {
    uint16_t keycode;
    uint64_t bits;
} Key_Binding;

typedef struct Emitted_Text {
    char *utf8;
} Emitted_Text;

struct Steno {
    Key_Binding *bindings;
    Emitted_Text *history;
    Dictionary dictionary;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    bool enabled;
    bool toggle_esc_down;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    void *send_userdata;
};

static bool load_keymap(Steno *steno, const char *path)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read keymap '%s'\n", path);
        return false;
    }

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
            free(file);
            return false;
        }

        uint16_t keycode = 0;
        uint64_t bits = 0;
        if (!platform_keycode_from_name(key_name, &keycode)) {
            fprintf(stderr, "stoin: unknown key name on keymap line %d: %s\n", line_number, key_name);
            free(file);
            return false;
        }
        if (!stroke_string_to_bits(steno_name, &bits)) {
            fprintf(stderr, "stoin: invalid steno stroke on keymap line %d: %s\n", line_number, steno_name);
            free(file);
            return false;
        }

        Key_Binding binding = {
            .keycode = keycode,
            .bits = bits,
        };
        arrput(steno->bindings, binding);
        ++line_number;
    }

    free(file);
    return arrlenu(steno->bindings) > 0;
}

static const Key_Binding *find_binding(const Steno *steno, uint16_t keycode)
{
    for (size_t i = 0; i < arrlenu(steno->bindings); ++i) {
        if (steno->bindings[i].keycode == keycode) {
            return &steno->bindings[i];
        }
    }
    return NULL;
}

static void reset_chord(Steno *steno)
{
    steno->down_keycodes = 0;
    steno->chord_bits = 0;
}

static bool emit_text(Steno *steno, const char *text)
{
    char *emitted = NULL;
    for (const char *p = text; *p != '\0'; ++p) {
        arrput(emitted, *p);
    }

    if (steno->spacing.mode == SPACING_MODE_AFTER_WORD && steno->spacing.spacing_char != '\0') {
        arrput(emitted, steno->spacing.spacing_char);
    }
    arrput(emitted, '\0');

    if (!steno->send_text(emitted, steno->send_userdata)) {
        arrfree(emitted);
        return false;
    }

    Emitted_Text history_entry = {
        .utf8 = emitted,
    };
    arrput(steno->history, history_entry);

    return true;
}

static bool undo_last_translation(Steno *steno)
{
    const size_t history_count = arrlenu(steno->history);
    if (history_count == 0) {
        return true;
    }

    Emitted_Text *entry = &steno->history[history_count - 1];
    if (!steno->delete_text(entry->utf8, steno->send_userdata)) {
        return false;
    }

    arrfree(entry->utf8);
    arrsetlen(steno->history, history_count - 1);
    return true;
}

static bool execute_command(Steno *steno, const char *command)
{
    if (strcmp(command, "=undo") == 0) {
        return undo_last_translation(steno);
    }

    fprintf(stderr, "stoin: unknown dictionary command '%s'\n", command);
    return true;
}

static bool emit_chord(Steno *steno)
{
    if (steno->chord_bits == 0) {
        return true;
    }

    const char *translation = dictionary_lookup_bits(&steno->dictionary, steno->chord_bits);
    if (translation != NULL) {
        if (translation[0] == '=') {
            return execute_command(steno, translation);
        }
        return emit_text(steno, translation);
    }

    char raw_chord[64] = {0};
    if (!chord_bits_to_string(steno->chord_bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }
    return emit_text(steno, raw_chord);
}

Steno *steno_create(const Steno_Config *config)
{
    if (config == NULL || config->send_text == NULL || config->delete_text == NULL) {
        return NULL;
    }

    Steno *steno = calloc(1, sizeof(*steno));
    if (steno == NULL) {
        return NULL;
    }

    steno->enabled = true;
    steno->spacing = (Spacing_State) {
        .mode = SPACING_MODE_AFTER_WORD,
        .spacing_char = ' ',
    };
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_userdata = config->send_userdata;

    if (!load_keymap(steno, config->keymap_path)) {
        steno_destroy(steno);
        return NULL;
    }

    if (!dictionary_load(&steno->dictionary, config->dictionary_path)) {
        steno_destroy(steno);
        return NULL;
    }

    return steno;
}

void steno_destroy(Steno *steno)
{
    if (steno == NULL) {
        return;
    }

    arrfree(steno->bindings);
    for (size_t i = 0; i < arrlenu(steno->history); ++i) {
        arrfree(steno->history[i].utf8);
    }
    arrfree(steno->history);
    dictionary_destroy(&steno->dictionary);
    free(steno);
}

bool steno_handle_event(Steno *steno, const Input_Event *event)
{
    if (steno == NULL || event == NULL) {
        return false;
    }

    const bool toggle_event = event->keycode == 53 && (event->control || steno->toggle_esc_down);
    if (toggle_event) {
        if (event->is_down && !steno->toggle_esc_down) {
            steno->enabled = !steno->enabled;
            reset_chord(steno);
            fprintf(stderr, "stoin: steno capture %s\n", steno->enabled ? "enabled" : "disabled");
        }
        steno->toggle_esc_down = event->is_down;
        return true;
    }

    if (!steno->enabled || event->command || event->control || event->option) {
        return false;
    }

    const Key_Binding *binding = find_binding(steno, event->keycode);
    if (binding == NULL) {
        return false;
    }

    if (event->keycode >= 64) {
        return false;
    }

    const uint64_t physical_bit = UINT64_C(1) << event->keycode;
    if (event->is_down) {
        if ((steno->down_keycodes & physical_bit) == 0 && !event->is_repeat) {
            steno->down_keycodes |= physical_bit;
            steno->chord_bits |= binding->bits;
        }
        return true;
    }

    steno->down_keycodes &= ~physical_bit;
    if (steno->down_keycodes == 0) {
        (void)emit_chord(steno);
        reset_chord(steno);
    }
    return true;
}

size_t steno_key_binding_count(const Steno *steno)
{
    return steno == NULL ? 0 : arrlenu(steno->bindings);
}

size_t steno_dictionary_count(const Steno *steno)
{
    return steno == NULL ? 0 : dictionary_count(&steno->dictionary);
}

bool steno_lookup_stroke(const Steno *steno, const char *stroke, const char **out_translation)
{
    if (steno == NULL) {
        return false;
    }
    return dictionary_lookup_stroke(&steno->dictionary, stroke, out_translation);
}

bool steno_dump_dictionary_json(const Steno *steno, const char *path)
{
    if (steno == NULL) {
        return false;
    }
    return dictionary_dump_json(&steno->dictionary, path);
}
