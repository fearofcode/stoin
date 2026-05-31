#include "steno.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef enum Stoin_Steno_Key {
    STOIN_STENO_NUM = 0,
    STOIN_STENO_LEFT_S,
    STOIN_STENO_LEFT_T,
    STOIN_STENO_LEFT_K,
    STOIN_STENO_LEFT_P,
    STOIN_STENO_LEFT_W,
    STOIN_STENO_LEFT_H,
    STOIN_STENO_LEFT_R,
    STOIN_STENO_A,
    STOIN_STENO_O,
    STOIN_STENO_STAR,
    STOIN_STENO_E,
    STOIN_STENO_U,
    STOIN_STENO_RIGHT_F,
    STOIN_STENO_RIGHT_R,
    STOIN_STENO_RIGHT_P,
    STOIN_STENO_RIGHT_B,
    STOIN_STENO_RIGHT_L,
    STOIN_STENO_RIGHT_G,
    STOIN_STENO_RIGHT_T,
    STOIN_STENO_RIGHT_S,
    STOIN_STENO_RIGHT_D,
    STOIN_STENO_RIGHT_Z,
    STOIN_STENO_KEY_COUNT,
} Stoin_Steno_Key;

typedef struct Stoin_Key_Binding {
    uint16_t keycode;
    uint64_t bit;
} Stoin_Key_Binding;

typedef struct Stoin_Dictionary_Entry {
    uint64_t key;
    char *value;
} Stoin_Dictionary_Entry;

typedef struct Stoin_Dump_Entry {
    char stroke[64];
    const char *translation;
} Stoin_Dump_Entry;

struct Stoin_Steno {
    Stoin_Key_Binding *bindings;
    Stoin_Dictionary_Entry *dictionary;
    uint64_t down_keycodes;
    uint64_t chord_bits;
    bool enabled;
    bool toggle_esc_down;
    Stoin_Steno_Send_Text_Fn send_text;
    void *send_userdata;
};

static uint64_t steno_bit(Stoin_Steno_Key key)
{
    return UINT64_C(1) << (uint64_t)key;
}

static char *read_entire_file(const char *path, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long end = ftell(file);
    if (end < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *data = malloc((size_t)end + 1);
    if (data == NULL) {
        fclose(file);
        return NULL;
    }

    const size_t bytes_read = fread(data, 1, (size_t)end, file);
    fclose(file);

    if (bytes_read != (size_t)end) {
        free(data);
        return NULL;
    }

    data[bytes_read] = '\0';
    if (out_size != NULL) {
        *out_size = bytes_read;
    }
    return data;
}

static char *copy_range(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *copy_cstring(const char *s)
{
    return copy_range(s, strlen(s));
}

static bool steno_token_to_bit(const char *token, uint64_t *out_bit)
{
    if (strcmp(token, "#") == 0) {
        *out_bit = steno_bit(STOIN_STENO_NUM);
    } else if (strcmp(token, "S") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_S);
    } else if (strcmp(token, "T") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_T);
    } else if (strcmp(token, "K") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_K);
    } else if (strcmp(token, "P") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_P);
    } else if (strcmp(token, "W") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_W);
    } else if (strcmp(token, "H") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_H);
    } else if (strcmp(token, "R") == 0) {
        *out_bit = steno_bit(STOIN_STENO_LEFT_R);
    } else if (strcmp(token, "A") == 0) {
        *out_bit = steno_bit(STOIN_STENO_A);
    } else if (strcmp(token, "O") == 0) {
        *out_bit = steno_bit(STOIN_STENO_O);
    } else if (strcmp(token, "*") == 0) {
        *out_bit = steno_bit(STOIN_STENO_STAR);
    } else if (strcmp(token, "E") == 0) {
        *out_bit = steno_bit(STOIN_STENO_E);
    } else if (strcmp(token, "U") == 0) {
        *out_bit = steno_bit(STOIN_STENO_U);
    } else if (strcmp(token, "-F") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_F);
    } else if (strcmp(token, "-R") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_R);
    } else if (strcmp(token, "-P") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_P);
    } else if (strcmp(token, "-B") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_B);
    } else if (strcmp(token, "-L") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_L);
    } else if (strcmp(token, "-G") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_G);
    } else if (strcmp(token, "-T") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_T);
    } else if (strcmp(token, "-S") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_S);
    } else if (strcmp(token, "-D") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_D);
    } else if (strcmp(token, "-Z") == 0) {
        *out_bit = steno_bit(STOIN_STENO_RIGHT_Z);
    } else {
        return false;
    }

    return true;
}

static uint64_t left_bit_for_char(char c)
{
    switch (c) {
    case '#': return steno_bit(STOIN_STENO_NUM);
    case 'S': return steno_bit(STOIN_STENO_LEFT_S);
    case 'T': return steno_bit(STOIN_STENO_LEFT_T);
    case 'K': return steno_bit(STOIN_STENO_LEFT_K);
    case 'P': return steno_bit(STOIN_STENO_LEFT_P);
    case 'W': return steno_bit(STOIN_STENO_LEFT_W);
    case 'H': return steno_bit(STOIN_STENO_LEFT_H);
    case 'R': return steno_bit(STOIN_STENO_LEFT_R);
    default: return 0;
    }
}

static uint64_t vowel_bit_for_char(char c)
{
    switch (c) {
    case 'A': return steno_bit(STOIN_STENO_A);
    case 'O': return steno_bit(STOIN_STENO_O);
    case '*': return steno_bit(STOIN_STENO_STAR);
    case 'E': return steno_bit(STOIN_STENO_E);
    case 'U': return steno_bit(STOIN_STENO_U);
    default: return 0;
    }
}

static uint64_t right_bit_for_char(char c)
{
    switch (c) {
    case 'F': return steno_bit(STOIN_STENO_RIGHT_F);
    case 'R': return steno_bit(STOIN_STENO_RIGHT_R);
    case 'P': return steno_bit(STOIN_STENO_RIGHT_P);
    case 'B': return steno_bit(STOIN_STENO_RIGHT_B);
    case 'L': return steno_bit(STOIN_STENO_RIGHT_L);
    case 'G': return steno_bit(STOIN_STENO_RIGHT_G);
    case 'T': return steno_bit(STOIN_STENO_RIGHT_T);
    case 'S': return steno_bit(STOIN_STENO_RIGHT_S);
    case 'D': return steno_bit(STOIN_STENO_RIGHT_D);
    case 'Z': return steno_bit(STOIN_STENO_RIGHT_Z);
    default: return 0;
    }
}

static bool add_steno_bit(uint64_t *bits, uint64_t bit)
{
    if (bit == 0 || (*bits & bit) != 0) {
        return false;
    }
    *bits |= bit;
    return true;
}

static bool stroke_string_to_bits(const char *stroke, uint64_t *out_bits)
{
    enum Stroke_Region {
        STROKE_REGION_LEFT,
        STROKE_REGION_VOWEL,
        STROKE_REGION_RIGHT,
    };

    uint64_t bits = 0;
    enum Stroke_Region region = STROKE_REGION_LEFT;
    bool saw_any = false;

    for (const char *p = stroke; *p != '\0'; ++p) {
        const char c = *p;
        uint64_t bit = 0;

        if (c == '/') {
            return false;
        }

        if (c == '-') {
            region = STROKE_REGION_RIGHT;
            continue;
        }

        switch (region) {
        case STROKE_REGION_LEFT:
            bit = left_bit_for_char(c);
            if (bit == 0) {
                bit = vowel_bit_for_char(c);
                if (bit != 0) {
                    region = STROKE_REGION_VOWEL;
                }
            }
            if (bit == 0) {
                bit = right_bit_for_char(c);
                if (bit != 0) {
                    region = STROKE_REGION_RIGHT;
                }
            }
            break;
        case STROKE_REGION_VOWEL:
            bit = vowel_bit_for_char(c);
            if (bit == 0) {
                bit = right_bit_for_char(c);
                if (bit != 0) {
                    region = STROKE_REGION_RIGHT;
                }
            }
            break;
        case STROKE_REGION_RIGHT:
            bit = right_bit_for_char(c);
            break;
        }

        if (!add_steno_bit(&bits, bit)) {
            return false;
        }
        saw_any = true;
    }

    if (!saw_any) {
        return false;
    }

    *out_bits = bits;
    return true;
}

static bool append_char(char *out, size_t out_size, size_t *index, char c)
{
    if (*index + 1 >= out_size) {
        return false;
    }
    out[(*index)++] = c;
    out[*index] = '\0';
    return true;
}

static bool chord_bits_to_string(uint64_t bits, char *out, size_t out_size)
{
    size_t index = 0;
    out[0] = '\0';

    const struct {
        uint64_t bit;
        char label;
    } left_and_vowels[] = {
        { steno_bit(STOIN_STENO_NUM), '#' },
        { steno_bit(STOIN_STENO_LEFT_S), 'S' },
        { steno_bit(STOIN_STENO_LEFT_T), 'T' },
        { steno_bit(STOIN_STENO_LEFT_K), 'K' },
        { steno_bit(STOIN_STENO_LEFT_P), 'P' },
        { steno_bit(STOIN_STENO_LEFT_W), 'W' },
        { steno_bit(STOIN_STENO_LEFT_H), 'H' },
        { steno_bit(STOIN_STENO_LEFT_R), 'R' },
        { steno_bit(STOIN_STENO_A), 'A' },
        { steno_bit(STOIN_STENO_O), 'O' },
        { steno_bit(STOIN_STENO_STAR), '*' },
        { steno_bit(STOIN_STENO_E), 'E' },
        { steno_bit(STOIN_STENO_U), 'U' },
    };
    const struct {
        uint64_t bit;
        char label;
    } right[] = {
        { steno_bit(STOIN_STENO_RIGHT_F), 'F' },
        { steno_bit(STOIN_STENO_RIGHT_R), 'R' },
        { steno_bit(STOIN_STENO_RIGHT_P), 'P' },
        { steno_bit(STOIN_STENO_RIGHT_B), 'B' },
        { steno_bit(STOIN_STENO_RIGHT_L), 'L' },
        { steno_bit(STOIN_STENO_RIGHT_G), 'G' },
        { steno_bit(STOIN_STENO_RIGHT_T), 'T' },
        { steno_bit(STOIN_STENO_RIGHT_S), 'S' },
        { steno_bit(STOIN_STENO_RIGHT_D), 'D' },
        { steno_bit(STOIN_STENO_RIGHT_Z), 'Z' },
    };

    for (size_t i = 0; i < sizeof(left_and_vowels) / sizeof(left_and_vowels[0]); ++i) {
        if ((bits & left_and_vowels[i].bit) != 0 && !append_char(out, out_size, &index, left_and_vowels[i].label)) {
            return false;
        }
    }

    uint64_t right_bits = 0;
    for (size_t i = 0; i < sizeof(right) / sizeof(right[0]); ++i) {
        right_bits |= right[i].bit;
    }

    if ((bits & right_bits) != 0) {
        if (!append_char(out, out_size, &index, '-')) {
            return false;
        }
        for (size_t i = 0; i < sizeof(right) / sizeof(right[0]); ++i) {
            if ((bits & right[i].bit) != 0 && !append_char(out, out_size, &index, right[i].label)) {
                return false;
            }
        }
    }

    return index > 0;
}

static bool load_keymap(Stoin_Steno *steno, const char *path)
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
        char steno_name[16] = {0};
        if (sscanf(line, "%63s %15s", key_name, steno_name) != 2) {
            fprintf(stderr, "stoin: invalid keymap line %d: %s\n", line_number, line);
            free(file);
            return false;
        }

        uint16_t keycode = 0;
        uint64_t bit = 0;
        if (!stoin_platform_keycode_from_name(key_name, &keycode)) {
            fprintf(stderr, "stoin: unknown key name on keymap line %d: %s\n", line_number, key_name);
            free(file);
            return false;
        }
        if (!steno_token_to_bit(steno_name, &bit)) {
            fprintf(stderr, "stoin: unknown steno key on keymap line %d: %s\n", line_number, steno_name);
            free(file);
            return false;
        }

        Stoin_Key_Binding binding = {
            .keycode = keycode,
            .bit = bit,
        };
        arrput(steno->bindings, binding);
        ++line_number;
    }

    free(file);
    return arrlenu(steno->bindings) > 0;
}

static const char *skip_json_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static bool parse_json_string(const char **cursor, char **out_string)
{
    const char *p = *cursor;
    if (*p != '"') {
        return false;
    }
    ++p;

    char *result = NULL;
    while (*p != '\0' && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            switch (c) {
            case '"': arrput(result, '"'); break;
            case '\\': arrput(result, '\\'); break;
            case '/': arrput(result, '/'); break;
            case 'b': arrput(result, '\b'); break;
            case 'f': arrput(result, '\f'); break;
            case 'n': arrput(result, '\n'); break;
            case 'r': arrput(result, '\r'); break;
            case 't': arrput(result, '\t'); break;
            case 'u':
                // The generated dictionary is written as UTF-8, not ASCII escapes.
                arrfree(result);
                return false;
            default:
                arrfree(result);
                return false;
            }
        } else {
            arrput(result, (char)c);
        }
    }

    if (*p != '"') {
        arrfree(result);
        return false;
    }
    ++p;

    arrput(result, '\0');
    *cursor = p;
    *out_string = result;
    return true;
}

static bool load_dictionary(Stoin_Steno *steno, const char *dictionary_path)
{
    size_t size = 0;
    char *file = read_entire_file(dictionary_path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read dictionary '%s'\n", dictionary_path);
        return false;
    }

    const char *p = skip_json_ws(file);
    if (*p != '{') {
        fprintf(stderr, "stoin: dictionary '%s' is not a JSON object\n", dictionary_path);
        free(file);
        return false;
    }
    ++p;

    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            break;
        }

        char *stroke = NULL;
        char *translation = NULL;
        if (!parse_json_string(&p, &stroke)) {
            break;
        }
        p = skip_json_ws(p);
        if (*p != ':') {
            arrfree(stroke);
            break;
        }
        ++p;
        p = skip_json_ws(p);
        if (!parse_json_string(&p, &translation)) {
            arrfree(stroke);
            break;
        }

        uint64_t bits = 0;
        if (stroke_string_to_bits(stroke, &bits)
            && hmgeti(steno->dictionary, bits) < 0) {
            Stoin_Dictionary_Entry entry = {
                .key = bits,
                .value = copy_cstring(translation),
            };
            if (entry.value != NULL) {
                hmputs(steno->dictionary, entry);
            }
        }

        arrfree(stroke);
        arrfree(translation);

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            break;
        }
        break;
    }

    free(file);
    return hmlenu(steno->dictionary) > 0;
}

static const Stoin_Key_Binding *find_binding(const Stoin_Steno *steno, uint16_t keycode)
{
    for (size_t i = 0; i < arrlenu(steno->bindings); ++i) {
        if (steno->bindings[i].keycode == keycode) {
            return &steno->bindings[i];
        }
    }
    return NULL;
}

static void reset_chord(Stoin_Steno *steno)
{
    steno->down_keycodes = 0;
    steno->chord_bits = 0;
}

static bool emit_chord(Stoin_Steno *steno)
{
    if (steno->chord_bits == 0) {
        return true;
    }

    const char *translation = hmget(steno->dictionary, steno->chord_bits);
    if (translation != NULL) {
        return steno->send_text(translation, steno->send_userdata);
    }

    char raw_chord[64] = {0};
    if (!chord_bits_to_string(steno->chord_bits, raw_chord, sizeof(raw_chord))) {
        return false;
    }
    return steno->send_text(raw_chord, steno->send_userdata);
}

static int compare_dump_entries(const void *a, const void *b)
{
    const Stoin_Dump_Entry *entry_a = a;
    const Stoin_Dump_Entry *entry_b = b;
    return strcmp(entry_a->stroke, entry_b->stroke);
}

static bool write_json_string(FILE *file, const char *s)
{
    if (fputc('"', file) == EOF) {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", file) == EOF) return false;
            break;
        case '\\':
            if (fputs("\\\\", file) == EOF) return false;
            break;
        case '\b':
            if (fputs("\\b", file) == EOF) return false;
            break;
        case '\f':
            if (fputs("\\f", file) == EOF) return false;
            break;
        case '\n':
            if (fputs("\\n", file) == EOF) return false;
            break;
        case '\r':
            if (fputs("\\r", file) == EOF) return false;
            break;
        case '\t':
            if (fputs("\\t", file) == EOF) return false;
            break;
        default:
            if (*p < 0x20) {
                if (fprintf(file, "\\u%04x", *p) < 0) return false;
            } else if (fputc(*p, file) == EOF) {
                return false;
            }
            break;
        }
    }

    return fputc('"', file) != EOF;
}

Stoin_Steno *stoin_steno_create(const Stoin_Steno_Config *config)
{
    if (config == NULL || config->send_text == NULL) {
        return NULL;
    }

    Stoin_Steno *steno = calloc(1, sizeof(*steno));
    if (steno == NULL) {
        return NULL;
    }

    steno->enabled = true;
    steno->send_text = config->send_text;
    steno->send_userdata = config->send_userdata;

    if (!load_keymap(steno, config->keymap_path)) {
        stoin_steno_destroy(steno);
        return NULL;
    }

    if (!load_dictionary(steno, config->dictionary_path)) {
        stoin_steno_destroy(steno);
        return NULL;
    }

    return steno;
}

void stoin_steno_destroy(Stoin_Steno *steno)
{
    if (steno == NULL) {
        return;
    }

    arrfree(steno->bindings);
    for (ptrdiff_t i = 0; i < hmlen(steno->dictionary); ++i) {
        free(steno->dictionary[i].value);
    }
    hmfree(steno->dictionary);
    free(steno);
}

bool stoin_steno_handle_event(Stoin_Steno *steno, const Stoin_Input_Event *event)
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

    const Stoin_Key_Binding *binding = find_binding(steno, event->keycode);
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
            steno->chord_bits |= binding->bit;
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

size_t stoin_steno_key_binding_count(const Stoin_Steno *steno)
{
    return steno == NULL ? 0 : arrlenu(steno->bindings);
}

size_t stoin_steno_dictionary_count(const Stoin_Steno *steno)
{
    return steno == NULL ? 0 : hmlenu(steno->dictionary);
}

bool stoin_steno_lookup_stroke(const Stoin_Steno *steno, const char *stroke, const char **out_translation)
{
    uint64_t bits = 0;
    if (steno == NULL || stroke == NULL || out_translation == NULL || !stroke_string_to_bits(stroke, &bits)) {
        return false;
    }

    Stoin_Dictionary_Entry *dictionary = steno->dictionary;
    const Stoin_Dictionary_Entry *entry = hmgetp_null(dictionary, bits);
    if (entry == NULL) {
        return false;
    }

    *out_translation = entry->value;
    return true;
}

bool stoin_steno_dump_dictionary_json(const Stoin_Steno *steno, const char *path)
{
    if (steno == NULL || path == NULL) {
        return false;
    }

    Stoin_Dump_Entry *entries = NULL;
    for (ptrdiff_t i = 0; i < hmlen(steno->dictionary); ++i) {
        Stoin_Dump_Entry entry = {
            .translation = steno->dictionary[i].value,
        };
        if (chord_bits_to_string(steno->dictionary[i].key, entry.stroke, sizeof(entry.stroke))) {
            arrput(entries, entry);
        }
    }

    qsort(entries, arrlenu(entries), sizeof(entries[0]), compare_dump_entries);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        arrfree(entries);
        return false;
    }

    bool ok = fputs("{\n", file) != EOF;
    for (size_t i = 0; ok && i < arrlenu(entries); ++i) {
        ok = fputs("  ", file) != EOF
            && write_json_string(file, entries[i].stroke)
            && fputs(": ", file) != EOF
            && write_json_string(file, entries[i].translation)
            && fputs(i + 1 == arrlenu(entries) ? "\n" : ",\n", file) != EOF;
    }
    ok = ok && fputs("}\n", file) != EOF;

    if (fclose(file) != 0) {
        ok = false;
    }
    arrfree(entries);
    return ok;
}

typedef struct Stoin_Test_Output {
    char *text;
} Stoin_Test_Output;

static bool test_send_text(const char *utf8, void *userdata)
{
    Stoin_Test_Output *output = userdata;
    if (output->text != NULL && arrlenu(output->text) > 0) {
        arrpop(output->text);
    }
    for (const char *p = utf8; *p != '\0'; ++p) {
        arrput(output->text, *p);
    }
    arrput(output->text, '\0');
    return true;
}

static bool test_key_event(Stoin_Steno *steno, const char *key_name, bool is_down)
{
    uint16_t keycode = 0;
    if (!stoin_platform_keycode_from_name(key_name, &keycode)) {
        return false;
    }
    Stoin_Input_Event event = {
        .keycode = keycode,
        .is_down = is_down,
    };
    return stoin_steno_handle_event(steno, &event);
}

bool stoin_steno_run_self_test(const Stoin_Steno_Config *config)
{
    Stoin_Test_Output output = {0};
    Stoin_Steno_Config test_config = *config;
    test_config.send_text = test_send_text;
    test_config.send_userdata = &output;

    Stoin_Steno *steno = stoin_steno_create(&test_config);
    if (steno == NULL) {
        return false;
    }

    bool ok = true;
    uint64_t rr_bits = 0;
    char rr_string[64] = {0};
    ok = ok && stroke_string_to_bits("R-R", &rr_bits);
    ok = ok && chord_bits_to_string(rr_bits, rr_string, sizeof(rr_string));
    ok = ok && strcmp(rr_string, "R-R") == 0;

    ok = ok && test_key_event(steno, "a", true);
    ok = ok && test_key_event(steno, "a", false);
    ok = ok && output.text != NULL && strcmp(output.text, "#") == 0;

    const char *the = NULL;
    ok = ok && stoin_steno_lookup_stroke(steno, "-T", &the);
    ok = ok && the != NULL && strcmp(the, "the") == 0;

    arrfree(output.text);
    stoin_steno_destroy(steno);
    return ok;
}
