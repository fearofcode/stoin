#include "steno_internal.h"

#include "steno_stroke.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include "../third_party/stb_ds.h"

bool steno_set_spacing(Steno *steno, const char *spacing)
{
    if (steno == NULL) {
        return false;
    }

    char *copy = copy_cstring(spacing != NULL ? spacing : "");
    if (copy == NULL) {
        return false;
    }

    free(steno->spacing.spacing);
    steno->spacing.spacing = copy;
    steno->spacing.mode = SPACING_MODE_BEFORE_WORD;
    return true;
}

static void reset_chord(Steno *steno)
{
    steno->down_keycodes = 0;
    steno->chord_bits = 0;
}

enum {
    KEYCODE_ESCAPE = 53,
    KEYCODE_LEFT_COMMAND = 55,
    KEYCODE_RIGHT_COMMAND = 54,
    KEYCODE_LEFT_OPTION = 58,
    KEYCODE_RIGHT_OPTION = 61,
    KEYCODE_LEFT_CONTROL = 59,
    KEYCODE_RIGHT_CONTROL = 62,
    KEYCODE_LEFT_SHIFT = 56,
    KEYCODE_RIGHT_SHIFT = 60,
};

static uint64_t keycode_physical_bit(uint16_t keycode)
{
    return keycode < 64 ? UINT64_C(1) << keycode : 0;
}

static bool update_shortcut_modifier_state(Steno *steno, const Input_Event *event)
{
    switch (event->keycode) {
    case KEYCODE_LEFT_COMMAND:
    case KEYCODE_RIGHT_COMMAND:
        steno->command_down = event->is_down;
        return true;
    case KEYCODE_LEFT_OPTION:
    case KEYCODE_RIGHT_OPTION:
        steno->option_down = event->is_down;
        return true;
    case KEYCODE_LEFT_CONTROL:
    case KEYCODE_RIGHT_CONTROL:
        steno->control_down = event->is_down;
        return true;
    default:
        return false;
    }
}

bool steno_reload_dictionary(Steno *steno)
{
    return steno != NULL && dictionary_stack_reload(&steno->dictionary_stack);
}

bool steno_reload_dictionary_if_changed(Steno *steno)
{
    return steno != NULL && dictionary_stack_reload_if_changed(&steno->dictionary_stack);
}

static bool refresh_phrasing_stamp(Steno *steno)
{
    if (steno == NULL || steno->phrasing_path == NULL) {
        return true;
    }

    Platform_File_Stamp stamp = {0};
    if (!platform_file_stamp(steno->phrasing_path, &stamp)) {
        steno->phrasing_stamp_valid = false;
        return false;
    }

    steno->phrasing_stamp = stamp;
    steno->phrasing_stamp_valid = true;
    return true;
}

static bool phrasing_file_changed(Steno *steno, bool *out_changed)
{
    if (steno == NULL || out_changed == NULL) {
        return false;
    }

    *out_changed = false;
    if (steno->phrasing_path == NULL) {
        return true;
    }
    if (!steno->phrasing_stamp_valid) {
        *out_changed = true;
        return true;
    }

    Platform_File_Stamp stamp = {0};
    if (!platform_file_stamp(steno->phrasing_path, &stamp)) {
        steno->phrasing_stamp_valid = false;
        return false;
    }
    *out_changed = stamp.exists != steno->phrasing_stamp.exists
        || stamp.size != steno->phrasing_stamp.size
        || stamp.modified_time_ns != steno->phrasing_stamp.modified_time_ns;
    return true;
}

bool steno_reload_phrasing(Steno *steno)
{
    if (steno == NULL || steno->phrasing_path == NULL) {
        return true;
    }

    Phrasing *next = phrasing_load(steno->phrasing_path);
    if (next == NULL) {
        (void)refresh_phrasing_stamp(steno);
        if (!steno->phrasing_reload_error_reported) {
            fputs("stoin: phrasing changed but reload failed; keeping previous phrasing\n", stderr);
            steno->phrasing_reload_error_reported = true;
        }
        return false;
    }

    phrasing_destroy(steno->phrasing);
    steno->phrasing = next;
    if (!refresh_phrasing_stamp(steno)) {
        fputs("stoin: warning: reloaded phrasing, but failed to refresh phrasing file stamp\n", stderr);
    }

    steno->phrasing_reload_error_reported = false;
    fprintf(stderr, "stoin: reloaded phrasing from %s\n", steno->phrasing_path);
    return true;
}

bool steno_reload_phrasing_if_changed(Steno *steno)
{
    if (steno == NULL) {
        return false;
    }

    bool changed = false;
    if (!phrasing_file_changed(steno, &changed)) {
        if (!steno->phrasing_reload_error_reported) {
            fputs("stoin: failed to check phrasing file for changes\n", stderr);
            steno->phrasing_reload_error_reported = true;
        }
        return false;
    }

    if (!changed) {
        return true;
    }
    steno->phrasing_reload_error_reported = false;
    return steno_reload_phrasing(steno);
}

bool steno_get_dictionary_paths(const Steno *steno, const char *const **out_paths, size_t *out_path_count)
{
    return dictionary_stack_get_paths(
        steno == NULL ? NULL : &steno->dictionary_stack,
        out_paths,
        out_path_count
    );
}

bool steno_get_phrasing_path(const Steno *steno, const char **out_path)
{
    if (steno == NULL || out_path == NULL) {
        return false;
    }
    *out_path = steno->phrasing_path;
    return true;
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
    steno->session_active = true;
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_key_combination = config->send_key_combination;
    steno->send_userdata = config->send_userdata;
    steno->trace_file = config->trace_file;
    steno->suggestions_file = config->suggestions_file;
    steno->suggestion_log_file = config->suggestion_log_file;
    steno->print_suggestions = config->print_suggestions;

    if (!steno_set_spacing(steno, " ")
        || (config->keymap_path != NULL && !keymap_load(&steno->keymap, config->keymap_path))) {
        steno_destroy(steno);
        return NULL;
    }

    if (config->phrasing_path != NULL) {
        steno->phrasing_path = copy_cstring(config->phrasing_path);
        if (steno->phrasing_path == NULL) {
            steno_destroy(steno);
            return NULL;
        }
        steno->phrasing = phrasing_load(steno->phrasing_path);
        if (steno->phrasing == NULL) {
            steno_destroy(steno);
            return NULL;
        }
        if (!refresh_phrasing_stamp(steno)) {
            fputs("stoin: warning: failed to capture phrasing file stamp; hot reload may not work\n", stderr);
        }
    }

    if (!dictionary_stack_set_paths(
            &steno->dictionary_stack,
            config->dictionary_path,
            config->dictionary_paths,
            config->dictionary_enabled,
            config->dictionary_path_count)
        || !dictionary_stack_load(&steno->dictionary_stack)) {
        steno_destroy(steno);
        return NULL;
    }

    if (config->word_list_path != NULL && !orthography_load(&steno->orthography, config->word_list_path)) {
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

    keymap_destroy(&steno->keymap);
    for (size_t i = 0; i < arrlenu(steno->translations); ++i) {
        translation_destroy(&steno->translations[i]);
    }
    arrfree(steno->translations);
    orthography_destroy(&steno->orthography);
    phrasing_destroy(steno->phrasing);
    dictionary_stack_destroy(&steno->dictionary_stack);
    free(steno->lookup_phrase);
    free(steno->phrasing_path);
    free(steno->spacing.spacing);
    free(steno);
}

bool steno_handle_event(Steno *steno, const Input_Event *event)
{
    if (steno == NULL || event == NULL) {
        return false;
    }

    if (!steno->session_active) {
        return false;
    }

    const bool modifier_key_event = update_shortcut_modifier_state(steno, event);
    const bool shortcut_modifier_down = event->command
        || event->control
        || event->option
        || steno->command_down
        || steno->control_down
        || steno->option_down;

    const bool toggle_event = event->keycode == KEYCODE_ESCAPE
        && (event->control || steno->control_down || steno->toggle_esc_down);
    if (toggle_event) {
        if (event->is_down && !steno->toggle_esc_down) {
            steno->enabled = !steno->enabled;
            reset_chord(steno);
            fprintf(stderr, "stoin: steno capture %s\n", steno->enabled ? "enabled" : "disabled");
        }
        steno->toggle_esc_down = event->is_down;
        return true;
    }

    if (modifier_key_event || !steno->enabled || shortcut_modifier_down) {
        return false;
    }

    const Key_Binding *binding = keymap_find_binding(&steno->keymap, event->keycode);
    if (binding == NULL) {
        return false;
    }

    if (event->keycode >= 64) {
        return false;
    }

    const uint64_t physical_bit = keycode_physical_bit(event->keycode);
    if (event->is_down) {
        if ((steno->down_keycodes & physical_bit) == 0 && !event->is_repeat) {
            steno->down_keycodes |= physical_bit;
            steno->chord_bits |= binding->bits;
        }
        return true;
    }

    steno->down_keycodes &= ~physical_bit;
    if (steno->down_keycodes == 0) {
        Stroke_Input stroke = {
            .bits = steno->chord_bits,
        };
        (void)steno_translate_stroke_input(steno, stroke);
        reset_chord(steno);
    }
    return true;
}

bool steno_handle_stroke(Steno *steno, Stroke_Input stroke)
{
    if (steno == NULL) {
        return false;
    }
    if (!steno->session_active) {
        return false;
    }
    return steno_translate_stroke_input(steno, stroke);
}

bool steno_handle_stroke_bits(Steno *steno, uint64_t bits)
{
    if (steno == NULL) {
        return false;
    }
    Stroke_Input stroke = {
        .bits = bits,
    };
    return steno_handle_stroke(steno, stroke);
}

void steno_set_session_active(Steno *steno, bool active)
{
    if (steno == NULL || steno->session_active == active) {
        return;
    }

    steno->session_active = active;
    reset_chord(steno);
    steno->toggle_esc_down = false;
    steno->control_down = false;
    steno->option_down = false;
    steno->command_down = false;
}

size_t steno_key_binding_count(const Steno *steno)
{
    return steno == NULL ? 0 : keymap_binding_count(&steno->keymap);
}

size_t steno_dictionary_count(const Steno *steno)
{
    return steno == NULL ? 0 : dictionary_count(&steno->dictionary_stack.dictionary);
}

size_t steno_translation_history_stroke_count(const Steno *steno)
{
    return steno == NULL ? 0 : translation_history_stroke_count(steno->translations);
}

bool steno_lookup_stroke(Steno *steno, const char *stroke, const char **out_translation)
{
    if (steno == NULL || stroke == NULL || out_translation == NULL) {
        return false;
    }
    if (dictionary_lookup_stroke(
            &steno->dictionary_stack.dictionary,
            stroke,
            out_translation)) {
        return true;
    }

    uint64_t bits = 0;
    if (!stroke_string_to_bits(stroke, &bits)) {
        return false;
    }
    free(steno->lookup_phrase);
    steno->lookup_phrase = NULL;
    const Phrase_Lookup_Result phrase_result = phrasing_lookup(
        steno->phrasing,
        bits,
        &steno->lookup_phrase
    );
    if (phrase_result == PHRASE_LOOKUP_HIT) {
        *out_translation = steno->lookup_phrase;
        return true;
    }
    return false;
}

bool steno_dump_dictionary_json(const Steno *steno, const char *path)
{
    if (steno == NULL) {
        return false;
    }
    return dictionary_dump_json(&steno->dictionary_stack.dictionary, path);
}
