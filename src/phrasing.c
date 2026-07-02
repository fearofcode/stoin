#include "phrasing.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

#include "../stb_ds.h"

static bool append_word(char **out, const char *word)
{
    if (out == NULL || word == NULL || word[0] == '\0') {
        return true;
    }

    if (*out != NULL && (*out)[0] != '\0' && !text_append_char(out, ' ')) {
        return false;
    }
    return text_append_cstring(out, word);
}

static const char *iv_tail_lookup(uint64_t bits)
{
    const uint64_t f = steno_bit(STENO_RIGHT_F);
    const uint64_t r = steno_bit(STENO_RIGHT_R);
    const uint64_t p = steno_bit(STENO_RIGHT_P);
    const uint64_t b = steno_bit(STENO_RIGHT_B);
    const uint64_t l = steno_bit(STENO_RIGHT_L);
    const uint64_t t = steno_bit(STENO_RIGHT_T);
    const uint64_t s = steno_bit(STENO_RIGHT_S);
    const uint64_t z = steno_bit(STENO_RIGHT_Z);

    if (bits == 0) return "";
    if (bits == t) return "the";
    if (bits == b) return "a";
    if (bits == (p | b)) return "an";
    if (bits == p) return "it";
    if (bits == (t | r)) return "that";
    if (bits == (t | s)) return "this";
    if (bits == (s | z)) return "these";
    if (bits == (t | z)) return "those";
    if (bits == (p | l)) return "me";
    if (bits == (r | p)) return "you";
    if (bits == r) return "your";
    if (bits == s) return "us";
    if (bits == (f | r)) return "her";
    if (bits == (f | l)) return "him";
    if (bits == (p | l | t)) return "them";
    if (bits == l) return "all";
    if (bits == (p | b | t)) return "one";
    return NULL;
}

static Phrase_Lookup_Result lookup_initial_verb(uint64_t bits, char **out_utf8)
{
    const uint64_t be_bits = steno_bit(STENO_LEFT_P) | steno_bit(STENO_LEFT_W);
    const uint64_t past_bit = steno_bit(STENO_RIGHT_D);
    const uint64_t tail_mask = steno_bit(STENO_RIGHT_F)
        | steno_bit(STENO_RIGHT_R)
        | steno_bit(STENO_RIGHT_P)
        | steno_bit(STENO_RIGHT_B)
        | steno_bit(STENO_RIGHT_L)
        | steno_bit(STENO_RIGHT_T)
        | steno_bit(STENO_RIGHT_S)
        | steno_bit(STENO_RIGHT_Z);
    const uint64_t allowed = be_bits | past_bit | tail_mask;

    if ((bits & ~allowed) != 0 || (bits & be_bits) != be_bits) {
        return PHRASE_LOOKUP_MISS;
    }

    const uint64_t tail_bits = bits & tail_mask;
    const char *tail = iv_tail_lookup(tail_bits);
    if (tail == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    char *text = NULL;
    const bool ok = append_word(&text, (bits & past_bit) != 0 ? "was" : "is")
        && append_word(&text, tail);
    if (!ok || text == NULL) {
        arrfree(text);
        return PHRASE_LOOKUP_ERROR;
    }

    *out_utf8 = copy_cstring(text);
    arrfree(text);
    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
}

Phrase_Lookup_Result phrasing_lookup(
    Phrase_Namespace namespace,
    uint64_t stroke_bits,
    char **out_utf8
)
{
    if (out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = NULL;

    switch (namespace) {
    case PHRASE_NAMESPACE_INITIAL_VERB:
        return lookup_initial_verb(stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONE:
    case PHRASE_NAMESPACE_NONVERB:
    case PHRASE_NAMESPACE_CORE_OPERATOR:
        return PHRASE_LOOKUP_MISS;
    default:
        return PHRASE_LOOKUP_ERROR;
    }
}
