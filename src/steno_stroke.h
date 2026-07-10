#ifndef STENO_STROKE_H
#define STENO_STROKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum Steno_Key {
    STENO_NUM = 0,
    STENO_LEFT_S,
    STENO_LEFT_T,
    STENO_LEFT_K,
    STENO_LEFT_P,
    STENO_LEFT_W,
    STENO_LEFT_H,
    STENO_LEFT_R,
    STENO_A,
    STENO_O,
    STENO_STAR,
    STENO_E,
    STENO_U,
    STENO_RIGHT_F,
    STENO_RIGHT_R,
    STENO_RIGHT_P,
    STENO_RIGHT_B,
    STENO_RIGHT_L,
    STENO_RIGHT_G,
    STENO_RIGHT_T,
    STENO_RIGHT_S,
    STENO_RIGHT_D,
    STENO_RIGHT_Z,
    STENO_KEY_COUNT,
} Steno_Key;

uint64_t steno_bit(Steno_Key key);
bool steno_token_to_bit(const char *token, uint64_t *out_bit);
bool stroke_string_to_bits(const char *stroke, uint64_t *out_bits);
bool chord_bits_to_string(uint64_t bits, char *out, size_t out_size);

#endif
