#include "phrasing.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../stb_ds.h"

typedef enum Phrase_Subject_Form {
    PHRASE_SUBJECT_EMPTY,
    PHRASE_SUBJECT_FIRST_SINGULAR,
    PHRASE_SUBJECT_SECOND_OR_PLURAL,
    PHRASE_SUBJECT_THIRD_SINGULAR,
} Phrase_Subject_Form;

typedef struct Phrase_Starter {
    const char *code;
    const char *text;
    Phrase_Subject_Form form;
} Phrase_Starter;

typedef struct Phrase_Verb {
    const char *code;
    const char *base;
    const char *present_3ps;
    const char *past;
    const char *present_participle;
    const char *past_participle;
    bool be;
} Phrase_Verb;

typedef struct Phrase_Grammar {
    bool past;
    bool negative;
    bool inverted;
    unsigned int aux;
    unsigned int aspect;
} Phrase_Grammar;

enum {
    PHRASE_AUX_NONE = 0,
    PHRASE_AUX_CAN = 1,
    PHRASE_AUX_SHOULD = 2,
    PHRASE_AUX_WILL = 3,

    PHRASE_ASPECT_SIMPLE = 0,
    PHRASE_ASPECT_PROGRESSIVE = 1,
    PHRASE_ASPECT_PERFECT = 2,
    PHRASE_ASPECT_PERFECT_PROGRESSIVE = 3,
};

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

static bool append_words3(char **out, const char *a, const char *b, const char *c)
{
    return append_word(out, a) && append_word(out, b) && append_word(out, c);
}

static uint64_t starter_code_bits(const char *code)
{
    uint64_t bits = 0;
    for (const char *p = code; p != NULL && *p != '\0'; ++p) {
        switch (*p) {
        case 'S': bits |= steno_bit(STENO_LEFT_S); break;
        case 'T': bits |= steno_bit(STENO_LEFT_T); break;
        case 'K': bits |= steno_bit(STENO_LEFT_K); break;
        case 'P': bits |= steno_bit(STENO_LEFT_P); break;
        case 'W': bits |= steno_bit(STENO_LEFT_W); break;
        default: return UINT64_MAX;
        }
    }
    return bits;
}

static uint64_t verb_code_bits(const char *code)
{
    uint64_t bits = 0;
    for (const char *p = code; p != NULL && *p != '\0'; ++p) {
        switch (*p) {
        case 'F': bits |= steno_bit(STENO_RIGHT_F); break;
        case 'R': bits |= steno_bit(STENO_RIGHT_R); break;
        case 'P': bits |= steno_bit(STENO_RIGHT_P); break;
        case 'B': bits |= steno_bit(STENO_RIGHT_B); break;
        case 'L': bits |= steno_bit(STENO_RIGHT_L); break;
        case 'G': bits |= steno_bit(STENO_RIGHT_G); break;
        default: return UINT64_MAX;
        }
    }
    return bits;
}

static bool starter_lookup(uint64_t bits, Phrase_Starter *out)
{
    static const Phrase_Starter starters[] = {
        { .code = "", .text = "", .form = PHRASE_SUBJECT_EMPTY },
        { .code = "S", .text = "I", .form = PHRASE_SUBJECT_FIRST_SINGULAR },
        { .code = "W", .text = "we", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL },
        { .code = "K", .text = "he", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "SK", .text = "she", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "P", .text = "it", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "T", .text = "they", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL },
        { .code = "ST", .text = "that", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "PW", .text = "you", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL },
        { .code = "TP", .text = "this", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "TK", .text = "there", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "TKW", .text = "there", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL },
        { .code = "SW", .text = "", .form = PHRASE_SUBJECT_SECOND_OR_PLURAL },
        { .code = "KW", .text = "who", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "TW", .text = "what", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "SP", .text = "someone", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "SPW", .text = "something", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "KP", .text = "everyone", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "KPW", .text = "everything", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "SKP", .text = "nobody", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
        { .code = "SKPW", .text = "nothing", .form = PHRASE_SUBJECT_THIRD_SINGULAR },
    };

    for (size_t i = 0; i < sizeof(starters) / sizeof(starters[0]); ++i) {
        if (starter_code_bits(starters[i].code) == bits) {
            *out = starters[i];
            return true;
        }
    }
    return false;
}

static bool starter_is_empty(Phrase_Starter starter)
{
    return starter.form == PHRASE_SUBJECT_EMPTY;
}

static bool subject_is_3ps(Phrase_Starter starter)
{
    return starter.form == PHRASE_SUBJECT_EMPTY
        || starter.form == PHRASE_SUBJECT_THIRD_SINGULAR;
}

static bool subject_is_first_singular(Phrase_Starter starter)
{
    return starter.form == PHRASE_SUBJECT_FIRST_SINGULAR;
}

static bool verb_lookup(uint64_t bits, const Phrase_Verb **out)
{
    static const Phrase_Verb verbs[] = {
        { .code = "F", .base = "have", .present_3ps = "has", .past = "had", .present_participle = "having", .past_participle = "had" },
        { .code = "R", .base = "run", .present_3ps = "runs", .past = "ran", .present_participle = "running", .past_participle = "run" },
        { .code = "P", .base = "want", .present_3ps = "wants", .past = "wanted", .present_participle = "wanting", .past_participle = "wanted" },
        { .code = "B", .base = "be", .present_3ps = "is", .past = "was", .present_participle = "being", .past_participle = "been", .be = true },
        { .code = "L", .base = "look", .present_3ps = "looks", .past = "looked", .present_participle = "looking", .past_participle = "looked" },
        { .code = "G", .base = "go", .present_3ps = "goes", .past = "went", .present_participle = "going", .past_participle = "gone" },
        { .code = "FR", .base = "see", .present_3ps = "sees", .past = "saw", .present_participle = "seeing", .past_participle = "seen" },
        { .code = "FP", .base = "happen", .present_3ps = "happens", .past = "happened", .present_participle = "happening", .past_participle = "happened" },
        { .code = "FB", .base = "say", .present_3ps = "says", .past = "said", .present_participle = "saying", .past_participle = "said" },
        { .code = "FL", .base = "feel", .present_3ps = "feels", .past = "felt", .present_participle = "feeling", .past_participle = "felt" },
        { .code = "FG", .base = "come", .present_3ps = "comes", .past = "came", .present_participle = "coming", .past_participle = "come" },
        { .code = "RP", .base = "do", .present_3ps = "does", .past = "did", .present_participle = "doing", .past_participle = "done" },
        { .code = "RB", .base = "ask", .present_3ps = "asks", .past = "asked", .present_participle = "asking", .past_participle = "asked" },
        { .code = "RL", .base = "recall", .present_3ps = "recalls", .past = "recalled", .present_participle = "recalling", .past_participle = "recalled" },
        { .code = "RG", .base = "forget", .present_3ps = "forgets", .past = "forgot", .present_participle = "forgetting", .past_participle = "forgotten" },
        { .code = "PB", .base = "know", .present_3ps = "knows", .past = "knew", .present_participle = "knowing", .past_participle = "known" },
        { .code = "PL", .base = "move", .present_3ps = "moves", .past = "moved", .present_participle = "moving", .past_participle = "moved" },
        { .code = "PG", .base = "get", .present_3ps = "gets", .past = "got", .present_participle = "getting", .past_participle = "got" },
        { .code = "BL", .base = "believe", .present_3ps = "believes", .past = "believed", .present_participle = "believing", .past_participle = "believed" },
        { .code = "BG", .base = "become", .present_3ps = "becomes", .past = "became", .present_participle = "becoming", .past_participle = "become" },
        { .code = "LG", .base = "love", .present_3ps = "loves", .past = "loved", .present_participle = "loving", .past_participle = "loved" },
        { .code = "FRP", .base = "read", .present_3ps = "reads", .past = "read", .present_participle = "reading", .past_participle = "read" },
        { .code = "FRB", .base = "care", .present_3ps = "cares", .past = "cared", .present_participle = "caring", .past_participle = "cared" },
        { .code = "FRPB", .base = "try", .present_3ps = "tries", .past = "tried", .present_participle = "trying", .past_participle = "tried" },
        { .code = "FRL", .base = "change", .present_3ps = "changes", .past = "changed", .present_participle = "changing", .past_participle = "changed" },
        { .code = "FRG", .base = "consider", .present_3ps = "considers", .past = "considered", .present_participle = "considering", .past_participle = "considered" },
        { .code = "FPB", .base = "expect", .present_3ps = "expects", .past = "expected", .present_participle = "expecting", .past_participle = "expected" },
        { .code = "FPL", .base = "hope", .present_3ps = "hopes", .past = "hoped", .present_participle = "hoping", .past_participle = "hoped" },
        { .code = "FPG", .base = "hear", .present_3ps = "hears", .past = "heard", .present_participle = "hearing", .past_participle = "heard" },
        { .code = "FBL", .base = "keep", .present_3ps = "keeps", .past = "kept", .present_participle = "keeping", .past_participle = "kept" },
        { .code = "FBG", .base = "learn", .present_3ps = "learns", .past = "learned", .present_participle = "learning", .past_participle = "learned" },
        { .code = "FLG", .base = "leave", .present_3ps = "leaves", .past = "left", .present_participle = "leaving", .past_participle = "left" },
        { .code = "RPB", .base = "understand", .present_3ps = "understands", .past = "understood", .present_participle = "understanding", .past_participle = "understood" },
        { .code = "RPL", .base = "remember", .present_3ps = "remembers", .past = "remembered", .present_participle = "remembering", .past_participle = "remembered" },
        { .code = "RPG", .base = "need", .present_3ps = "needs", .past = "needed", .present_participle = "needing", .past_participle = "needed" },
        { .code = "RBL", .base = "take", .present_3ps = "takes", .past = "took", .present_participle = "taking", .past_participle = "taken" },
        { .code = "RBG", .base = "work", .present_3ps = "works", .past = "worked", .present_participle = "working", .past_participle = "worked" },
        { .code = "RLG", .base = "realize", .present_3ps = "realizes", .past = "realized", .present_participle = "realizing", .past_participle = "realized" },
        { .code = "PBL", .base = "mean", .present_3ps = "means", .past = "meant", .present_participle = "meaning", .past_participle = "meant" },
        { .code = "PBG", .base = "think", .present_3ps = "thinks", .past = "thought", .present_participle = "thinking", .past_participle = "thought" },
        { .code = "PLG", .base = "imagine", .present_3ps = "imagines", .past = "imagined", .present_participle = "imagining", .past_participle = "imagined" },
        { .code = "BLG", .base = "like", .present_3ps = "likes", .past = "liked", .present_participle = "liking", .past_participle = "liked" },
        { .code = "FRPL", .base = "wish", .present_3ps = "wishes", .past = "wished", .present_participle = "wishing", .past_participle = "wished" },
        { .code = "FRPBL", .base = "use", .present_3ps = "uses", .past = "used", .present_participle = "using", .past_participle = "used" },
        { .code = "FRPG", .base = "give", .present_3ps = "gives", .past = "gave", .present_participle = "giving", .past_participle = "given" },
        { .code = "FRBL", .base = "let", .present_3ps = "lets", .past = "let", .present_participle = "letting", .past_participle = "let" },
        { .code = "FRBG", .base = "tell", .present_3ps = "tells", .past = "told", .present_participle = "telling", .past_participle = "told" },
        { .code = "FRLG", .base = "live", .present_3ps = "lives", .past = "lived", .present_participle = "living", .past_participle = "lived" },
        { .code = "FPBL", .base = "mind", .present_3ps = "minds", .past = "minded", .present_participle = "minding", .past_participle = "minded" },
        { .code = "FPBG", .base = "put", .present_3ps = "puts", .past = "put", .present_participle = "putting", .past_participle = "put" },
        { .code = "FPLG", .base = "set", .present_3ps = "sets", .past = "set", .present_participle = "setting", .past_participle = "set" },
        { .code = "FBLG", .base = "seem", .present_3ps = "seems", .past = "seemed", .present_participle = "seeming", .past_participle = "seemed" },
        { .code = "RPBL", .base = "make", .present_3ps = "makes", .past = "made", .present_participle = "making", .past_participle = "made" },
        { .code = "RPBG", .base = "show", .present_3ps = "shows", .past = "showed", .present_participle = "showing", .past_participle = "shown" },
        { .code = "RPLG", .base = "remain", .present_3ps = "remains", .past = "remained", .present_participle = "remaining", .past_participle = "remained" },
        { .code = "RBLG", .base = "call", .present_3ps = "calls", .past = "called", .present_participle = "calling", .past_participle = "called" },
        { .code = "PBLG", .base = "find", .present_3ps = "finds", .past = "found", .present_participle = "finding", .past_participle = "found" },
    };

    for (size_t i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i) {
        if (verb_code_bits(verbs[i].code) == bits) {
            *out = &verbs[i];
            return true;
        }
    }
    return false;
}

static const char *tail_lookup(uint64_t bits)
{
    const uint64_t t = steno_bit(STENO_RIGHT_T);
    const uint64_t s = steno_bit(STENO_RIGHT_S);
    const uint64_t d = steno_bit(STENO_RIGHT_D);
    const uint64_t z = steno_bit(STENO_RIGHT_Z);

    if (bits == 0) return "";
    if (bits == t) return "the";
    if (bits == s) return "a";
    if (bits == d) return "it";
    if (bits == z) return "that";
    if (bits == (t | s)) return "this";
    if (bits == (t | d)) return "me";
    if (bits == (t | z)) return "those";
    if (bits == (s | d)) return "her";
    if (bits == (s | z)) return "us";
    if (bits == (d | z)) return "them";
    if (bits == (t | s | d)) return "you";
    if (bits == (t | s | z)) return "these";
    if (bits == (t | d | z)) return "him";
    if (bits == (s | d | z)) return "one";
    if (bits == (t | s | d | z)) return "all";
    return NULL;
}

static const char *finite_be(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        if (subject_is_3ps(starter) || subject_is_first_singular(starter)) {
            return negative ? "wasn't" : "was";
        }
        return negative ? "weren't" : "were";
    }

    if (subject_is_first_singular(starter)) {
        return negative ? "am not" : "am";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "isn't" : "is";
    }
    return negative ? "aren't" : "are";
}

static const char *finite_have(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        return negative ? "hadn't" : "had";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "hasn't" : "has";
    }
    return negative ? "haven't" : "have";
}

static const char *do_support(Phrase_Starter starter, bool past, bool negative)
{
    if (past) {
        return negative ? "didn't" : "did";
    }
    if (subject_is_3ps(starter)) {
        return negative ? "doesn't" : "does";
    }
    return negative ? "don't" : "do";
}

static const char *aux_word(const Phrase_Grammar *grammar)
{
    switch (grammar->aux) {
    case PHRASE_AUX_CAN: return grammar->past ? "could" : "can";
    case PHRASE_AUX_SHOULD: return "should";
    case PHRASE_AUX_WILL: return grammar->past ? "would" : "will";
    default: return "";
    }
}

static const char *negative_aux_word(const Phrase_Grammar *grammar)
{
    switch (grammar->aux) {
    case PHRASE_AUX_CAN: return grammar->past ? "couldn't" : "can't";
    case PHRASE_AUX_SHOULD: return "shouldn't";
    case PHRASE_AUX_WILL: return grammar->past ? "wouldn't" : "won't";
    default: return "";
    }
}

static const char *simple_verb_form(Phrase_Starter starter, const Phrase_Verb *verb, bool past)
{
    if (verb->be) {
        return finite_be(starter, past, false);
    }
    if (past) {
        return verb->past;
    }
    return subject_is_3ps(starter) ? verb->present_3ps : verb->base;
}

static bool append_simple_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_word(out, verb->base);
    }

    if (verb->be) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative));
    }

    if (grammar->negative) {
        return append_word(out, do_support(starter, grammar->past, true))
            && append_word(out, verb->base);
    }

    return append_word(out, simple_verb_form(starter, verb, grammar->past));
}

static bool append_progressive_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "be", verb->present_participle, NULL);
    }
    return append_word(out, finite_be(starter, grammar->past, grammar->negative))
        && append_word(out, verb->present_participle);
}

static bool append_perfect_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "have", verb->past_participle, NULL);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_word(out, verb->past_participle);
}

static bool append_perfect_progressive_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aux != PHRASE_AUX_NONE) {
        return append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            && append_words3(out, "have", "been", verb->present_participle);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_words3(out, "been", verb->present_participle, NULL);
}

static bool append_bare_aux_complement(
    char **out,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    switch (grammar->aspect) {
    case PHRASE_ASPECT_SIMPLE:
        return append_word(out, verb->base);
    case PHRASE_ASPECT_PROGRESSIVE:
        return append_words3(out, "be", verb->present_participle, NULL);
    case PHRASE_ASPECT_PERFECT:
        return append_words3(out, "have", verb->past_participle, NULL);
    case PHRASE_ASPECT_PERFECT_PROGRESSIVE:
        return append_words3(out, "have", "been", verb->present_participle);
    default:
        return false;
    }
}

static bool grammar_is_simple_infinitive(const Phrase_Grammar *grammar)
{
    return grammar != NULL
        && !grammar->past
        && !grammar->negative
        && !grammar->inverted
        && grammar->aux == PHRASE_AUX_NONE
        && grammar->aspect == PHRASE_ASPECT_SIMPLE;
}

static bool append_infinitive_predicate(char **out, const Phrase_Verb *verb)
{
    return append_word(out, "to") && append_word(out, verb == NULL ? NULL : verb->base);
}

static bool append_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    switch (grammar->aspect) {
    case PHRASE_ASPECT_SIMPLE:
        return append_simple_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PROGRESSIVE:
        return append_progressive_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PERFECT:
        return append_perfect_predicate(out, starter, grammar, verb);
    case PHRASE_ASPECT_PERFECT_PROGRESSIVE:
        return append_perfect_progressive_predicate(out, starter, grammar, verb);
    default:
        return false;
    }
}

static bool append_inverted_predicate(
    char **out,
    Phrase_Starter starter,
    const Phrase_Grammar *grammar,
    const Phrase_Verb *verb
)
{
    if (grammar->aspect == PHRASE_ASPECT_SIMPLE && grammar->aux == PHRASE_AUX_NONE && !verb->be) {
        return append_word(out, do_support(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->base);
    }

    if (grammar->aspect == PHRASE_ASPECT_SIMPLE && grammar->aux == PHRASE_AUX_NONE && verb->be) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text);
    }

    if (grammar->aux != PHRASE_AUX_NONE) {
        if (!append_word(out, grammar->negative ? negative_aux_word(grammar) : aux_word(grammar))
            || !append_word(out, starter.text)) {
            return false;
        }
        return append_bare_aux_complement(out, grammar, verb);
    }

    if (grammar->aspect == PHRASE_ASPECT_PROGRESSIVE) {
        return append_word(out, finite_be(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->present_participle);
    }
    if (grammar->aspect == PHRASE_ASPECT_PERFECT) {
        return append_word(out, finite_have(starter, grammar->past, grammar->negative))
            && append_word(out, starter.text)
            && append_word(out, verb->past_participle);
    }
    return append_word(out, finite_have(starter, grammar->past, grammar->negative))
        && append_word(out, starter.text)
        && append_word(out, "been")
        && append_word(out, verb->present_participle);
}

static bool grammar_decode(uint64_t bits, Phrase_Grammar *out)
{
    const uint64_t h = steno_bit(STENO_LEFT_H);
    const uint64_t r = steno_bit(STENO_LEFT_R);
    const uint64_t a = steno_bit(STENO_A);
    const uint64_t o = steno_bit(STENO_O);
    const uint64_t star = steno_bit(STENO_STAR);
    const uint64_t e = steno_bit(STENO_E);
    const uint64_t u = steno_bit(STENO_U);

    Phrase_Grammar grammar = {
        .past = (bits & h) != 0,
        .negative = (bits & star) != 0,
        .inverted = (bits & u) != 0,
        .aux = 0,
        .aspect = 0,
    };
    *out = grammar;

    if ((bits & a) != 0) {
        out->aux |= PHRASE_AUX_CAN;
    }
    if ((bits & o) != 0) {
        out->aux |= PHRASE_AUX_SHOULD;
    }
    if ((bits & e) != 0) {
        out->aspect |= PHRASE_ASPECT_PROGRESSIVE;
    }
    if ((bits & r) != 0) {
        out->aspect |= PHRASE_ASPECT_PERFECT;
    }
    return true;
}

static Phrase_Lookup_Result lookup_core(uint64_t bits, char **out_utf8)
{
    const uint64_t starter_mask = steno_bit(STENO_LEFT_S)
        | steno_bit(STENO_LEFT_T)
        | steno_bit(STENO_LEFT_K)
        | steno_bit(STENO_LEFT_P)
        | steno_bit(STENO_LEFT_W);
    const uint64_t grammar_mask = steno_bit(STENO_LEFT_H)
        | steno_bit(STENO_LEFT_R)
        | steno_bit(STENO_A)
        | steno_bit(STENO_O)
        | steno_bit(STENO_STAR)
        | steno_bit(STENO_E)
        | steno_bit(STENO_U);
    const uint64_t verb_mask = steno_bit(STENO_RIGHT_F)
        | steno_bit(STENO_RIGHT_R)
        | steno_bit(STENO_RIGHT_P)
        | steno_bit(STENO_RIGHT_B)
        | steno_bit(STENO_RIGHT_L)
        | steno_bit(STENO_RIGHT_G);
    const uint64_t tail_mask = steno_bit(STENO_RIGHT_T)
        | steno_bit(STENO_RIGHT_S)
        | steno_bit(STENO_RIGHT_D)
        | steno_bit(STENO_RIGHT_Z);

    if ((bits & ~(starter_mask | grammar_mask | verb_mask | tail_mask)) != 0) {
        return PHRASE_LOOKUP_MISS;
    }

    Phrase_Starter starter = {0};
    const uint64_t starter_bits = bits & starter_mask;
    if (!starter_lookup(starter_bits, &starter)) {
        return PHRASE_LOOKUP_MISS;
    }

    Phrase_Grammar grammar = {0};
    if (!grammar_decode(bits & grammar_mask, &grammar)) {
        return PHRASE_LOOKUP_ERROR;
    }

    const Phrase_Verb *verb = NULL;
    const uint64_t verb_bits = bits & verb_mask;
    if (!verb_lookup(verb_bits, &verb)) {
        return PHRASE_LOOKUP_MISS;
    }

    const char *tail = tail_lookup(bits & tail_mask);
    if (tail == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    char *text = NULL;
    bool ok = true;
    if (starter_is_empty(starter) && grammar_is_simple_infinitive(&grammar)) {
        ok = append_infinitive_predicate(&text, verb);
    } else if (grammar.inverted) {
        ok = append_inverted_predicate(&text, starter, &grammar, verb);
    } else {
        ok = append_word(&text, starter.text)
            && append_predicate(&text, starter, &grammar, verb);
    }
    ok = ok && append_word(&text, tail);

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
    case PHRASE_NAMESPACE_CORE:
        return lookup_core(stroke_bits, out_utf8);
    case PHRASE_NAMESPACE_NONE:
    case PHRASE_NAMESPACE_NONVERB:
    case PHRASE_NAMESPACE_CORE_OPERATOR:
        return PHRASE_LOOKUP_MISS;
    default:
        return PHRASE_LOOKUP_ERROR;
    }
}
