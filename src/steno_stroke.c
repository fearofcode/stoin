#include "steno_stroke.h"

#include <string.h>

uint64_t steno_bit(Steno_Key key)
{
    return UINT64_C(1) << (uint64_t)key;
}

bool steno_token_to_bit(const char *token, uint64_t *out_bit)
{
    if (strcmp(token, "#") == 0) {
        *out_bit = steno_bit(STENO_NUM);
    } else if (strcmp(token, "S") == 0) {
        *out_bit = steno_bit(STENO_LEFT_S);
    } else if (strcmp(token, "T") == 0) {
        *out_bit = steno_bit(STENO_LEFT_T);
    } else if (strcmp(token, "K") == 0) {
        *out_bit = steno_bit(STENO_LEFT_K);
    } else if (strcmp(token, "P") == 0) {
        *out_bit = steno_bit(STENO_LEFT_P);
    } else if (strcmp(token, "W") == 0) {
        *out_bit = steno_bit(STENO_LEFT_W);
    } else if (strcmp(token, "H") == 0) {
        *out_bit = steno_bit(STENO_LEFT_H);
    } else if (strcmp(token, "R") == 0) {
        *out_bit = steno_bit(STENO_LEFT_R);
    } else if (strcmp(token, "A") == 0) {
        *out_bit = steno_bit(STENO_A);
    } else if (strcmp(token, "O") == 0) {
        *out_bit = steno_bit(STENO_O);
    } else if (strcmp(token, "*") == 0) {
        *out_bit = steno_bit(STENO_STAR);
    } else if (strcmp(token, "E") == 0) {
        *out_bit = steno_bit(STENO_E);
    } else if (strcmp(token, "U") == 0) {
        *out_bit = steno_bit(STENO_U);
    } else if (strcmp(token, "-F") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_F);
    } else if (strcmp(token, "-R") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_R);
    } else if (strcmp(token, "-P") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_P);
    } else if (strcmp(token, "-B") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_B);
    } else if (strcmp(token, "-L") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_L);
    } else if (strcmp(token, "-G") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_G);
    } else if (strcmp(token, "-T") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_T);
    } else if (strcmp(token, "-S") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_S);
    } else if (strcmp(token, "-D") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_D);
    } else if (strcmp(token, "-Z") == 0) {
        *out_bit = steno_bit(STENO_RIGHT_Z);
    } else {
        return false;
    }

    return true;
}

static uint64_t left_bit_for_char(char c)
{
    switch (c) {
    case '#': return steno_bit(STENO_NUM);
    case 'S': return steno_bit(STENO_LEFT_S);
    case 'T': return steno_bit(STENO_LEFT_T);
    case 'K': return steno_bit(STENO_LEFT_K);
    case 'P': return steno_bit(STENO_LEFT_P);
    case 'W': return steno_bit(STENO_LEFT_W);
    case 'H': return steno_bit(STENO_LEFT_H);
    case 'R': return steno_bit(STENO_LEFT_R);
    default: return 0;
    }
}

static uint64_t vowel_bit_for_char(char c)
{
    switch (c) {
    case 'A': return steno_bit(STENO_A);
    case 'O': return steno_bit(STENO_O);
    case '*': return steno_bit(STENO_STAR);
    case 'E': return steno_bit(STENO_E);
    case 'U': return steno_bit(STENO_U);
    default: return 0;
    }
}

static uint64_t right_bit_for_char(char c)
{
    switch (c) {
    case 'F': return steno_bit(STENO_RIGHT_F);
    case 'R': return steno_bit(STENO_RIGHT_R);
    case 'P': return steno_bit(STENO_RIGHT_P);
    case 'B': return steno_bit(STENO_RIGHT_B);
    case 'L': return steno_bit(STENO_RIGHT_L);
    case 'G': return steno_bit(STENO_RIGHT_G);
    case 'T': return steno_bit(STENO_RIGHT_T);
    case 'S': return steno_bit(STENO_RIGHT_S);
    case 'D': return steno_bit(STENO_RIGHT_D);
    case 'Z': return steno_bit(STENO_RIGHT_Z);
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

bool stroke_string_to_bits(const char *stroke, uint64_t *out_bits)
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
                if (bit != 0) region = STROKE_REGION_VOWEL;
            }
            if (bit == 0) {
                bit = right_bit_for_char(c);
                if (bit != 0) region = STROKE_REGION_RIGHT;
            }
            break;
        case STROKE_REGION_VOWEL:
            bit = vowel_bit_for_char(c);
            if (bit == 0) {
                bit = right_bit_for_char(c);
                if (bit != 0) region = STROKE_REGION_RIGHT;
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

bool chord_bits_to_string(uint64_t bits, char *out, size_t out_size)
{
    size_t index = 0;
    out[0] = '\0';

    const struct {
        uint64_t bit;
        char label;
    } left_and_vowels[] = {
        { steno_bit(STENO_NUM), '#' },
        { steno_bit(STENO_LEFT_S), 'S' },
        { steno_bit(STENO_LEFT_T), 'T' },
        { steno_bit(STENO_LEFT_K), 'K' },
        { steno_bit(STENO_LEFT_P), 'P' },
        { steno_bit(STENO_LEFT_W), 'W' },
        { steno_bit(STENO_LEFT_H), 'H' },
        { steno_bit(STENO_LEFT_R), 'R' },
        { steno_bit(STENO_A), 'A' },
        { steno_bit(STENO_O), 'O' },
        { steno_bit(STENO_STAR), '*' },
        { steno_bit(STENO_E), 'E' },
        { steno_bit(STENO_U), 'U' },
    };
    const struct {
        uint64_t bit;
        char label;
    } right[] = {
        { steno_bit(STENO_RIGHT_F), 'F' },
        { steno_bit(STENO_RIGHT_R), 'R' },
        { steno_bit(STENO_RIGHT_P), 'P' },
        { steno_bit(STENO_RIGHT_B), 'B' },
        { steno_bit(STENO_RIGHT_L), 'L' },
        { steno_bit(STENO_RIGHT_G), 'G' },
        { steno_bit(STENO_RIGHT_T), 'T' },
        { steno_bit(STENO_RIGHT_S), 'S' },
        { steno_bit(STENO_RIGHT_D), 'D' },
        { steno_bit(STENO_RIGHT_Z), 'Z' },
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
