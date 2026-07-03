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
    const uint64_t g = steno_bit(STENO_RIGHT_G);
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
    if (bits == (r | b)) return "she";
    if (bits == (r | b | l)) return "she will";
    if (bits == (r | b | l | t)) return "she'll";
    if (bits == (r | p | b)) return "he";
    if (bits == (r | p | b | l)) return "he will";
    if (bits == (r | p | b | l | t)) return "he'll";
    if (bits == (g | t)) return "going to";
    if (bits == g) return "give";
    if (bits == (b | g | t)) return "why";
    if (bits == (r | p | l)) return "who";
    if (bits == (b | l | g)) return "what";
    if (bits == (p | b | g)) return "when";
    if (bits == (r | l | g)) return "where";
    if (bits == (p | l | g)) return "how";
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
        | steno_bit(STENO_RIGHT_G)
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

static const char *nv_head_lookup(uint64_t bits)
{
    const uint64_t f = steno_bit(STENO_RIGHT_F);
    const uint64_t r = steno_bit(STENO_RIGHT_R);
    const uint64_t p = steno_bit(STENO_RIGHT_P);
    const uint64_t b = steno_bit(STENO_RIGHT_B);
    const uint64_t l = steno_bit(STENO_RIGHT_L);
    const uint64_t g = steno_bit(STENO_RIGHT_G);
    const uint64_t t = steno_bit(STENO_RIGHT_T);
    const uint64_t s = steno_bit(STENO_RIGHT_S);
    const uint64_t z = steno_bit(STENO_RIGHT_Z);

    if (bits == t) return "the";
    if (bits == b) return "a";
    if (bits == (p | b)) return "an";
    if (bits == f) return "if";
    if (bits == p) return "it";
    if (bits == (t | r)) return "that";
    if (bits == (t | s)) return "this";
    if (bits == (s | z)) return "these";
    if (bits == (t | z)) return "those";
    if (bits == (r | p | b)) return "he";
    if (bits == (r | b)) return "she";
    if (bits == (p | l)) return "me";
    if (bits == (r | p)) return "you";
    if (bits == r) return "your";
    if (bits == s) return "us";
    if (bits == (f | r)) return "her";
    if (bits == (f | l)) return "him";
    if (bits == (p | l | t)) return "them";
    if (bits == l) return "all";
    if (bits == (p | b | t)) return "one";
    if (bits == (r | p | l)) return "who";
    if (bits == (b | l | g)) return "what";
    if (bits == (p | b | g)) return "when";
    if (bits == (r | l | g)) return "where";
    if (bits == (b | g | t)) return "why";
    if (bits == (p | l | g)) return "how";
    return NULL;
}

static Phrase_Lookup_Result copy_phrase_result(const char *phrase, char **out_utf8)
{
    if (phrase == NULL) {
        return PHRASE_LOOKUP_MISS;
    }
    *out_utf8 = copy_cstring(phrase);
    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
}

static const char *lookup_nonverb_fixed(uint64_t left_bits, uint64_t right_bits)
{
    const uint64_t left_t = steno_bit(STENO_LEFT_T);
    const uint64_t left_k = steno_bit(STENO_LEFT_K);
    const uint64_t left_p = steno_bit(STENO_LEFT_P);

    const uint64_t f = steno_bit(STENO_RIGHT_F);
    const uint64_t r = steno_bit(STENO_RIGHT_R);
    const uint64_t p = steno_bit(STENO_RIGHT_P);
    const uint64_t b = steno_bit(STENO_RIGHT_B);
    const uint64_t l = steno_bit(STENO_RIGHT_L);
    const uint64_t g = steno_bit(STENO_RIGHT_G);

    if (left_bits == 0) {
        if (right_bits == f) return "anything else";
        if (right_bits == r) return "something else";
        if (right_bits == p) return "everybody else";
        if (right_bits == b) return "everyone else";
        if (right_bits == l) return "everything else";
        if (right_bits == g) return "nothing else";
        if (right_bits == (f | r)) return "no one else";
        return NULL;
    }

    if (left_bits == left_t) {
        if (right_bits == f) return "each of the";
        if (right_bits == r) return "both of the";
        if (right_bits == p) return "one of them";
        if (right_bits == b) return "some of them";
        if (right_bits == l) return "any of them";
        if (right_bits == g) return "all of them";
        return NULL;
    }

    if (left_bits == left_k) {
        if (right_bits == r) return "as though";
        if (right_bits == l) return "even though";
        if (right_bits == g) return "assuming that";
        if (right_bits == (f | r)) return "provided that";
        if (right_bits == (f | p)) return "except that";
        if (right_bits == (f | b)) return "in case";
        if (right_bits == (f | l)) return "because of that";
        return NULL;
    }

    if (left_bits == left_p) {
        if (right_bits == f) return "in that";
        if (right_bits == r) return "in order to";
        if (right_bits == p) return "so as to";
        if (right_bits == b) return "instead of";
        if (right_bits == l) return "not only";
        if (right_bits == g) return "not yet";
        if (right_bits == (f | r)) return "up to";
        if (right_bits == (f | p)) return "as to whether";
        return NULL;
    }

    return NULL;
}

static const char *lookup_nonverb_family(uint64_t left_bits)
{
    const uint64_t s = steno_bit(STENO_LEFT_S);
    const uint64_t t = steno_bit(STENO_LEFT_T);
    const uint64_t p = steno_bit(STENO_LEFT_P);
    const uint64_t w = steno_bit(STENO_LEFT_W);
    const uint64_t h = steno_bit(STENO_LEFT_H);
    const uint64_t r = steno_bit(STENO_LEFT_R);

    if (left_bits == (t | p | h | r)) return "unless";
    if (left_bits == (t | p | h)) return "even";
    if (left_bits == w) return "with";
    if (left_bits == s) return "as";
    return NULL;
}

static Phrase_Lookup_Result lookup_nonverb(uint64_t bits, char **out_utf8)
{
    const uint64_t left_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W)
        | steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R);
    const uint64_t right_mask = steno_bit(STENO_RIGHT_F)
        | steno_bit(STENO_RIGHT_R)
        | steno_bit(STENO_RIGHT_P)
        | steno_bit(STENO_RIGHT_B)
        | steno_bit(STENO_RIGHT_L)
        | steno_bit(STENO_RIGHT_G)
        | steno_bit(STENO_RIGHT_T)
        | steno_bit(STENO_RIGHT_S)
        | steno_bit(STENO_RIGHT_Z);
    const uint64_t allowed = left_mask | right_mask;
    if ((bits & ~allowed) != 0) {
        return PHRASE_LOOKUP_MISS;
    }

    const uint64_t left_bits = bits & left_mask;
    const uint64_t right_bits = bits & right_mask;

    const char *fixed = lookup_nonverb_fixed(left_bits, right_bits);
    if (fixed != NULL) {
        return copy_phrase_result(fixed, out_utf8);
    }

    const char *family = lookup_nonverb_family(left_bits);
    const char *head = nv_head_lookup(right_bits);
    if (family == NULL || head == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    char *text = NULL;
    const bool ok = append_word(&text, family) && append_word(&text, head);
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
    case PHRASE_NAMESPACE_NONVERB:
        return lookup_nonverb(stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONE:
    case PHRASE_NAMESPACE_CORE_OPERATOR:
        return PHRASE_LOOKUP_MISS;
    default:
        return PHRASE_LOOKUP_ERROR;
    }
}
