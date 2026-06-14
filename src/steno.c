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

typedef struct Translation Translation;
struct Translation {
    uint64_t *strokes;
    char *utf8;
    Translation *replaced;
};

enum {
    MAX_TRANSLATION_STROKES = 100,
    MAX_TRANSLATION_OUTLINE_BYTES = 4096,
};

struct Steno {
    Key_Binding *bindings;
    Translation *translations;
    Dictionary dictionary;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    bool enabled;
    bool session_active;
    bool toggle_esc_down;
    Spacing_State spacing;
    Send_Text_Fn send_text;
    Delete_Text_Fn delete_text;
    void *send_userdata;
    FILE *trace_file;
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

static char *format_emitted_text(const Steno *steno, const char *text)
{
    char *emitted = NULL;
    for (const char *p = text; *p != '\0'; ++p) {
        arrput(emitted, *p);
    }

    if (steno->spacing.mode == SPACING_MODE_AFTER_WORD && steno->spacing.spacing_char != '\0') {
        arrput(emitted, steno->spacing.spacing_char);
    }
    arrput(emitted, '\0');

    return emitted;
}

static void translation_destroy(Translation *translation)
{
    if (translation == NULL) {
        return;
    }

    arrfree(translation->strokes);
    arrfree(translation->utf8);
    for (size_t i = 0; i < arrlenu(translation->replaced); ++i) {
        translation_destroy(&translation->replaced[i]);
    }
    arrfree(translation->replaced);
    memset(translation, 0, sizeof(*translation));
}

static bool translation_set_strokes(Translation *translation, const uint64_t *strokes, size_t stroke_count)
{
    if (translation == NULL || strokes == NULL || stroke_count == 0) {
        return false;
    }

    for (size_t i = 0; i < stroke_count; ++i) {
        arrput(translation->strokes, strokes[i]);
    }
    return true;
}

static bool append_string(char **out, const char *s)
{
    if (out == NULL || s == NULL) {
        return false;
    }

    if (*out != NULL && arrlenu(*out) > 0) {
        arrpop(*out);
    }
    for (const char *p = s; *p != '\0'; ++p) {
        arrput(*out, *p);
    }
    arrput(*out, '\0');
    return true;
}

static char *translation_range_text(const Translation *translations, size_t start, size_t count)
{
    char *text = NULL;
    arrput(text, '\0');
    for (size_t i = 0; i < count; ++i) {
        if (!append_string(&text, translations[start + i].utf8)) {
            arrfree(text);
            return NULL;
        }
    }
    return text;
}

static char *translation_replaced_text(const Translation *translation)
{
    if (translation == NULL) {
        return NULL;
    }
    return translation_range_text(translation->replaced, 0, arrlenu(translation->replaced));
}

static size_t common_utf8_prefix_bytes(const char *a, const char *b)
{
    size_t index = 0;
    size_t last_boundary = 0;

    while (a[index] != '\0' && b[index] != '\0' && a[index] == b[index]) {
        ++index;
        if (((unsigned char)a[index] & 0xC0) != 0x80) {
            last_boundary = index;
        }
    }

    return last_boundary;
}

static bool replace_output_text(Steno *steno, const char *old_text, const char *new_text)
{
    const size_t prefix = common_utf8_prefix_bytes(old_text, new_text);
    const char *delete_suffix = old_text + prefix;
    const char *insert_suffix = new_text + prefix;

    if (delete_suffix[0] != '\0' && !steno->delete_text(delete_suffix, steno->send_userdata)) {
        return false;
    }
    if (insert_suffix[0] != '\0' && !steno->send_text(insert_suffix, steno->send_userdata)) {
        return false;
    }

    return true;
}

static bool undo_last_translation(Steno *steno)
{
    const size_t translation_count = arrlenu(steno->translations);
    if (translation_count == 0) {
        return true;
    }

    Translation translation = steno->translations[translation_count - 1];
    char *replacement_text = translation_replaced_text(&translation);
    if (replacement_text == NULL) {
        return false;
    }

    if (!replace_output_text(steno, translation.utf8, replacement_text)) {
        arrfree(replacement_text);
        return false;
    }

    arrsetlen(steno->translations, translation_count - 1);
    for (size_t i = 0; i < arrlenu(translation.replaced); ++i) {
        arrput(steno->translations, translation.replaced[i]);
    }

    arrsetlen(translation.replaced, 0);
    translation_destroy(&translation);
    arrfree(replacement_text);
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

static void trace_stroke(const Steno *steno, const char *raw_chord, const char *translation)
{
    if (steno == NULL || steno->trace_file == NULL || raw_chord == NULL) {
        return;
    }

    if (translation != NULL) {
        fprintf(steno->trace_file, "%s -> %s\n", raw_chord, translation);
    } else {
        fprintf(steno->trace_file, "%s -> [untranslated]\n", raw_chord);
    }
    fflush(steno->trace_file);
}

static bool stroke_sequence_to_string(const uint64_t *strokes, size_t stroke_count, char *out, size_t out_size)
{
    if (strokes == NULL || stroke_count == 0 || out == NULL || out_size == 0) {
        return false;
    }

    size_t index = 0;
    out[0] = '\0';

    for (size_t i = 0; i < stroke_count; ++i) {
        char stroke[64] = {0};
        if (!chord_bits_to_string(strokes[i], stroke, sizeof(stroke))) {
            return false;
        }

        const size_t separator_length = i == 0 ? 0 : 1;
        const size_t stroke_length = strlen(stroke);
        if (index + separator_length + stroke_length >= out_size) {
            return false;
        }
        if (i != 0) {
            out[index++] = '/';
        }
        memcpy(out + index, stroke, stroke_length + 1);
        index += stroke_length;
    }

    return true;
}

typedef struct Translation_Match {
    const char *translation;
    uint64_t strokes[MAX_TRANSLATION_STROKES];
    size_t stroke_count;
    size_t replaced_count;
    char outline[MAX_TRANSLATION_OUTLINE_BYTES];
} Translation_Match;

static void set_translation_match(
    Translation_Match *match,
    const char *translation,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count
)
{
    match->translation = translation;
    match->stroke_count = stroke_count;
    match->replaced_count = replaced_count;
    memcpy(match->strokes, strokes, stroke_count * sizeof(strokes[0]));
    (void)stroke_sequence_to_string(
        strokes,
        stroke_count,
        match->outline,
        sizeof(match->outline)
    );
}

static bool find_translation_match(Steno *steno, uint64_t bits, Translation_Match *out_match)
{
    if (steno == NULL || out_match == NULL) {
        return false;
    }

    size_t max_strokes = dictionary_longest_key(&steno->dictionary);
    if (max_strokes == 0 || max_strokes > MAX_TRANSLATION_STROKES) {
        max_strokes = MAX_TRANSLATION_STROKES;
    }

    uint64_t candidate[MAX_TRANSLATION_STROKES] = { bits };
    size_t candidate_count = 1;
    size_t replaced_count = 0;
    bool found = false;

    const char *translation = dictionary_lookup_strokes(&steno->dictionary, candidate, candidate_count);
    if (translation != NULL && translation[0] != '=') {
        set_translation_match(out_match, translation, candidate, candidate_count, replaced_count);
        found = true;
    }

    for (size_t i = arrlenu(steno->translations); i > 0 && candidate_count < max_strokes;) {
        --i;
        const Translation *previous = &steno->translations[i];
        const size_t previous_stroke_count = arrlenu(previous->strokes);
        if (previous_stroke_count == 0 || candidate_count + previous_stroke_count > max_strokes) {
            break;
        }

        memmove(
            candidate + previous_stroke_count,
            candidate,
            candidate_count * sizeof(candidate[0])
        );
        memcpy(candidate, previous->strokes, previous_stroke_count * sizeof(candidate[0]));
        candidate_count += previous_stroke_count;
        ++replaced_count;

        translation = dictionary_lookup_strokes(&steno->dictionary, candidate, candidate_count);
        if (translation != NULL && translation[0] != '=') {
            set_translation_match(out_match, translation, candidate, candidate_count, replaced_count);
            found = true;
        }
    }

    if (!found) {
        set_translation_match(out_match, NULL, &bits, 1, 0);
    }
    return true;
}

static bool apply_translation_match(Steno *steno, const Translation_Match *match)
{
    if (steno == NULL || match == NULL) {
        return false;
    }

    const char *translation_text = match->translation != NULL ? match->translation : match->outline;
    Translation next = {0};
    next.utf8 = format_emitted_text(steno, translation_text);
    if (next.utf8 == NULL || !translation_set_strokes(&next, match->strokes, match->stroke_count)) {
        translation_destroy(&next);
        return false;
    }

    const size_t translation_count = arrlenu(steno->translations);
    if (match->replaced_count > translation_count) {
        translation_destroy(&next);
        return false;
    }

    const size_t replace_start = translation_count - match->replaced_count;
    char *old_text = translation_range_text(steno->translations, replace_start, match->replaced_count);
    if (old_text == NULL) {
        translation_destroy(&next);
        return false;
    }

    if (!replace_output_text(steno, old_text, next.utf8)) {
        arrfree(old_text);
        translation_destroy(&next);
        return false;
    }

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, steno->translations[i]);
    }
    arrsetlen(steno->translations, replace_start);
    arrput(steno->translations, next);

    arrfree(old_text);
    return true;
}

static bool translate_chord_bits(Steno *steno, uint64_t bits)
{
    if (bits == 0) {
        return true;
    }

    char raw_chord[64] = {0};
    if (!chord_bits_to_string(bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }

    const char *single_stroke_translation = dictionary_lookup_bits(&steno->dictionary, bits);
    if (single_stroke_translation != NULL && single_stroke_translation[0] == '=') {
        trace_stroke(steno, raw_chord, single_stroke_translation);
        return execute_command(steno, single_stroke_translation);
    }

    Translation_Match match = {0};
    if (!find_translation_match(steno, bits, &match)) {
        return false;
    }

    trace_stroke(steno, match.outline, match.translation);
    return apply_translation_match(steno, &match);
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
    steno->spacing = (Spacing_State) {
        .mode = SPACING_MODE_AFTER_WORD,
        .spacing_char = ' ',
    };
    steno->send_text = config->send_text;
    steno->delete_text = config->delete_text;
    steno->send_userdata = config->send_userdata;
    steno->trace_file = config->trace_file;

    if (config->keymap_path != NULL && !load_keymap(steno, config->keymap_path)) {
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
    for (size_t i = 0; i < arrlenu(steno->translations); ++i) {
        translation_destroy(&steno->translations[i]);
    }
    arrfree(steno->translations);
    dictionary_destroy(&steno->dictionary);
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
        (void)translate_chord_bits(steno, steno->chord_bits);
        reset_chord(steno);
    }
    return true;
}

bool steno_handle_stroke_bits(Steno *steno, uint64_t bits)
{
    if (steno == NULL) {
        return false;
    }
    if (!steno->session_active) {
        return false;
    }
    return translate_chord_bits(steno, bits);
}

void steno_set_session_active(Steno *steno, bool active)
{
    if (steno == NULL || steno->session_active == active) {
        return;
    }

    steno->session_active = active;
    reset_chord(steno);
    steno->toggle_esc_down = false;
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
