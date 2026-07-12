#include "phrasing.h"

#include "steno_stroke.h"
#include "text_util.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"
#include "../third_party/cjson/cJSON.h"

typedef enum Fv_Agreement {
    FV_AGREEMENT_FIRST_SINGULAR,
    FV_AGREEMENT_THIRD_SINGULAR,
    FV_AGREEMENT_PLURAL,
} Fv_Agreement;

typedef enum Fv_Modal {
    FV_MODAL_NONE,
    FV_MODAL_CAN,
    FV_MODAL_SHOULD,
    FV_MODAL_WILL,
} Fv_Modal;

typedef enum Fv_Structure {
    FV_STRUCTURE_SIMPLE,
    FV_STRUCTURE_PROGRESSIVE,
    FV_STRUCTURE_PERFECT,
    FV_STRUCTURE_PERFECT_PROGRESSIVE,
} Fv_Structure;

typedef enum Fv_Verb_Kind {
    FV_VERB_OTHER,
    FV_VERB_BE,
    FV_VERB_HAVE,
    FV_VERB_DO,
} Fv_Verb_Kind;

typedef struct Phrase_Form {
    uint64_t bits;
    char *text;
} Phrase_Form;

typedef struct Iv_Stem {
    uint64_t bits;
    Phrase_Form *forms;
    size_t *tail_indices;
    bool has_tail_allowlist;
} Iv_Stem;

typedef struct Phrase_Tail {
    char *id;
    uint64_t bits;
    char *text;
} Phrase_Tail;

typedef struct Fv_Starter {
    uint64_t bits;
    Fv_Agreement agreement;
    char *text;
    char *be_contraction;
    char *have_contraction;
    char *will_contraction;
    size_t *ender_indices;
    bool has_ender_allowlist;
} Fv_Starter;

typedef struct Fv_Operator {
    uint64_t bits;
    Fv_Modal modal;
    bool negative;
} Fv_Operator;

typedef struct Fv_Structure_Row {
    uint64_t bits;
    Fv_Structure structure;
} Fv_Structure_Row;

typedef struct Fv_Verb {
    char *id;
    Fv_Verb_Kind kind;
    char *base;
    char *third;
    char *past;
    char *present_participle;
    char *past_participle;
} Fv_Verb;

typedef struct Fv_Ender {
    uint64_t bits;
    char *verb_id;
    const Fv_Verb *verb;
    char *suffix;
    bool past;
} Fv_Ender;

struct Phrasing {
    Iv_Stem *iv_stems;
    Phrase_Tail *iv_tails;
    Fv_Starter *fv_starters;
    Fv_Operator *fv_operators;
    Fv_Structure_Row *fv_structures;
    Fv_Verb *fv_verbs;
    Fv_Ender *fv_enders;
    uint64_t contraction_bits;
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

static Phrase_Lookup_Result copy_phrase_words(const char *first, const char *second, char **out_utf8)
{
    char *result = NULL;
    if (!append_word(&result, first) || !append_word(&result, second)) {
        arrfree(result);
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = copy_cstring(result != NULL ? result : "");
    arrfree(result);
    if (*out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    return PHRASE_LOOKUP_HIT;
}

static bool parse_stroke_string(const char *stroke, uint64_t *out_bits)
{
    if (stroke == NULL || out_bits == NULL) {
        return false;
    }
    if (stroke[0] == '\0') {
        *out_bits = 0;
        return true;
    }
    return stroke_string_to_bits(stroke, out_bits);
}

static void print_json_parse_error(const char *path, const char *file, const char *parse_end)
{
    size_t line = 1;
    size_t column = 1;
    if (file != NULL && parse_end != NULL) {
        for (const char *p = file; p < parse_end && *p != '\0'; ++p) {
            if (*p == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
    }
    fprintf(stderr, "stoin: phrasing '%s' has invalid JSON near line %zu, column %zu\n", path, line, column);
}

static void print_field_error(const char *path, const char *context, const char *field, const char *message)
{
    fprintf(stderr, "stoin: phrasing '%s' %s.%s %s\n", path, context, field, message);
}

static const cJSON *required_object(const cJSON *parent, const char *field, const char *path, const char *context)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (!cJSON_IsObject(item)) {
        print_field_error(path, context, field, "must be an object");
        return NULL;
    }
    return item;
}

static const cJSON *required_array(const cJSON *parent, const char *field, const char *path, const char *context)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (!cJSON_IsArray(item)) {
        print_field_error(path, context, field, "must be an array");
        return NULL;
    }
    return item;
}

static bool copy_optional_string(const cJSON *parent, const char *field, char **out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (item == NULL || cJSON_IsNull(item)) {
        *out = NULL;
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    *out = copy_cstring(item->valuestring);
    return *out != NULL;
}

static bool copy_required_string(const cJSON *parent, const char *field, char **out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    *out = copy_cstring(item->valuestring);
    return *out != NULL;
}

static bool parse_required_stroke(
    const cJSON *parent,
    const char *field,
    uint64_t *out_bits,
    const char *path,
    const char *context
)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        print_field_error(path, context, field, "must be a stroke string");
        return false;
    }
    if (!parse_stroke_string(item->valuestring, out_bits)) {
        fprintf(stderr, "stoin: phrasing '%s' %s.%s has invalid outline '%s'\n",
            path, context, field, item->valuestring);
        return false;
    }
    return true;
}

static bool report_duplicate_stroke(const char *path, const char *context, uint64_t bits)
{
    char stroke[64] = {0};
    if (bits == 0) {
        snprintf(stroke, sizeof(stroke), "<empty>");
    } else if (!chord_bits_to_string(bits, stroke, sizeof(stroke))) {
        snprintf(stroke, sizeof(stroke), "0x%llx", (unsigned long long)bits);
    }
    fprintf(stderr,
        "stoin: phrasing '%s' %s.stroke duplicates outline '%s' in the same list\n",
        path,
        context,
        stroke);
    return false;
}

static bool phrase_form_stroke_is_unique(
    const Phrase_Form *forms,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(forms); ++i) {
        if (forms[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool tail_stroke_is_unique(
    const Phrase_Tail *tails,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(tails); ++i) {
        if (tails[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool tail_id_is_unique(
    const Phrase_Tail *tails,
    const char *id,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(tails); ++i) {
        if (strcmp(tails[i].id, id) == 0) {
            fprintf(stderr,
                "stoin: phrasing '%s' %s.id duplicates tail id '%s'\n",
                path,
                context,
                id);
            return false;
        }
    }
    return true;
}

static bool iv_stem_stroke_is_unique(
    const Iv_Stem *stems,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(stems); ++i) {
        if (stems[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool fv_starter_stroke_is_unique(
    const Fv_Starter *starters,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(starters); ++i) {
        if (starters[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool fv_operator_stroke_is_unique(
    const Fv_Operator *operators,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(operators); ++i) {
        if (operators[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool fv_structure_stroke_is_unique(
    const Fv_Structure_Row *structures,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(structures); ++i) {
        if (structures[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool fv_ender_stroke_is_unique(
    const Fv_Ender *enders,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(enders); ++i) {
        if (enders[i].bits == bits) {
            return report_duplicate_stroke(path, context, bits);
        }
    }
    return true;
}

static bool parse_optional_bool(const cJSON *parent, const char *field, bool default_value, bool *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (item == NULL) {
        *out_value = default_value;
        return true;
    }
    if (!cJSON_IsBool(item)) {
        return false;
    }
    *out_value = cJSON_IsTrue(item);
    return true;
}

static bool parse_agreement(const char *s, Fv_Agreement *out)
{
    if (strcmp(s, "first_singular") == 0) {
        *out = FV_AGREEMENT_FIRST_SINGULAR;
        return true;
    }
    if (strcmp(s, "third_singular") == 0) {
        *out = FV_AGREEMENT_THIRD_SINGULAR;
        return true;
    }
    if (strcmp(s, "plural") == 0) {
        *out = FV_AGREEMENT_PLURAL;
        return true;
    }
    return false;
}

static bool parse_modal(const char *s, Fv_Modal *out)
{
    if (strcmp(s, "none") == 0) {
        *out = FV_MODAL_NONE;
        return true;
    }
    if (strcmp(s, "can") == 0) {
        *out = FV_MODAL_CAN;
        return true;
    }
    if (strcmp(s, "should") == 0) {
        *out = FV_MODAL_SHOULD;
        return true;
    }
    if (strcmp(s, "will") == 0) {
        *out = FV_MODAL_WILL;
        return true;
    }
    return false;
}

static bool parse_structure(const char *s, Fv_Structure *out)
{
    if (strcmp(s, "simple") == 0) {
        *out = FV_STRUCTURE_SIMPLE;
        return true;
    }
    if (strcmp(s, "progressive") == 0) {
        *out = FV_STRUCTURE_PROGRESSIVE;
        return true;
    }
    if (strcmp(s, "perfect") == 0) {
        *out = FV_STRUCTURE_PERFECT;
        return true;
    }
    if (strcmp(s, "perfect_progressive") == 0) {
        *out = FV_STRUCTURE_PERFECT_PROGRESSIVE;
        return true;
    }
    return false;
}

static Fv_Verb_Kind verb_kind_for_id(const char *id)
{
    if (strcmp(id, "be") == 0) return FV_VERB_BE;
    if (strcmp(id, "have") == 0) return FV_VERB_HAVE;
    if (strcmp(id, "do") == 0) return FV_VERB_DO;
    return FV_VERB_OTHER;
}

static const Fv_Verb *find_verb(const Phrasing *phrasing, const char *id)
{
    if (id == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < arrlenu(phrasing->fv_verbs); ++i) {
        if (strcmp(phrasing->fv_verbs[i].id, id) == 0) {
            return &phrasing->fv_verbs[i];
        }
    }
    return NULL;
}

static bool parse_phrase_form_array(
    Phrase_Form **out_forms,
    const cJSON *array,
    const char *path,
    const char *context
)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char item_context[128] = {0};
        snprintf(item_context, sizeof(item_context), "%s[%zu]", context, index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, item_context);
            return false;
        }

        Phrase_Form form = {0};
        if (!parse_required_stroke(item, "stroke", &form.bits, path, item_context)) {
            return false;
        }
        if (!phrase_form_stroke_is_unique(*out_forms, form.bits, path, item_context)) {
            return false;
        }
        if (!copy_required_string(item, "text", &form.text)) {
            print_field_error(path, item_context, "text", "must be a string");
            return false;
        }
        arrput(*out_forms, form);
        ++index;
    }
    return true;
}

static bool parse_tail_array(Phrase_Tail **out_tails, const cJSON *array, const char *path, const char *context)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char item_context[128] = {0};
        snprintf(item_context, sizeof(item_context), "%s[%zu]", context, index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, item_context);
            return false;
        }

        Phrase_Tail tail = {0};
        if (!copy_required_string(item, "id", &tail.id)) {
            print_field_error(path, item_context, "id", "must be a string");
            return false;
        }
        if (!tail_id_is_unique(*out_tails, tail.id, path, item_context)) {
            free(tail.id);
            return false;
        }
        if (!parse_required_stroke(item, "stroke", &tail.bits, path, item_context)) {
            free(tail.id);
            return false;
        }
        if (!tail_stroke_is_unique(*out_tails, tail.bits, path, item_context)) {
            free(tail.id);
            return false;
        }
        if (!copy_required_string(item, "text", &tail.text)) {
            print_field_error(path, item_context, "text", "must be a string");
            free(tail.id);
            return false;
        }
        arrput(*out_tails, tail);
        ++index;
    }
    return true;
}

static bool find_tail_index(const Phrase_Tail *tails, const char *id, size_t *out_index)
{
    for (size_t i = 0; i < arrlenu(tails); ++i) {
        if (strcmp(tails[i].id, id) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool find_ender_index(const Fv_Ender *enders, uint64_t bits, size_t *out_index)
{
    for (size_t i = 0; i < arrlenu(enders); ++i) {
        if (enders[i].bits == bits) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static void destroy_iv_stem_contents(Iv_Stem *stem)
{
    if (stem == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(stem->forms); ++i) {
        free(stem->forms[i].text);
    }
    arrfree(stem->forms);
    arrfree(stem->tail_indices);
}

static void destroy_fv_starter_contents(Fv_Starter *starter)
{
    if (starter == NULL) {
        return;
    }
    free(starter->text);
    free(starter->be_contraction);
    free(starter->have_contraction);
    free(starter->will_contraction);
    arrfree(starter->ender_indices);
}

static bool parse_iv_stem_tail_allowlist(
    Iv_Stem *stem,
    const cJSON *stem_object,
    const Phrase_Tail *tails,
    const char *path,
    const char *context
)
{
    const cJSON *allowlist = cJSON_GetObjectItemCaseSensitive(stem_object, "tails");
    if (allowlist == NULL) {
        return true;
    }

    stem->has_tail_allowlist = true;
    if (!cJSON_IsArray(allowlist)) {
        print_field_error(path, context, "tails", "must be an array");
        return false;
    }

    const cJSON *item = NULL;
    size_t item_index = 0;
    cJSON_ArrayForEach(item, allowlist) {
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            fprintf(stderr,
                "stoin: phrasing '%s' %s.tails[%zu] must be a string\n",
                path,
                context,
                item_index);
            return false;
        }

        size_t tail_index = 0;
        if (!find_tail_index(tails, item->valuestring, &tail_index)) {
            fprintf(stderr,
                "stoin: phrasing '%s' %s.tails[%zu] references unknown tail id '%s'\n",
                path,
                context,
                item_index,
                item->valuestring);
            return false;
        }
        for (size_t i = 0; i < arrlenu(stem->tail_indices); ++i) {
            if (stem->tail_indices[i] == tail_index) {
                fprintf(stderr,
                    "stoin: phrasing '%s' %s.tails[%zu] duplicates tail id '%s'\n",
                    path,
                    context,
                    item_index,
                    item->valuestring);
                return false;
            }
        }
        arrput(stem->tail_indices, tail_index);
        ++item_index;
    }
    return true;
}

static bool parse_initial_verbs(Phrasing *phrasing, const cJSON *root, const char *path)
{
    const cJSON *section = required_object(root, "initial_verbs", path, "root");
    if (section == NULL) {
        return false;
    }

    const cJSON *tails = required_array(section, "tails", path, "initial_verbs");
    const cJSON *stems = required_array(section, "stems", path, "initial_verbs");
    if (tails == NULL || stems == NULL || !parse_tail_array(&phrasing->iv_tails, tails, path, "initial_verbs.tails")) {
        return false;
    }

    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, stems) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "initial_verbs.stems[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Iv_Stem stem = {0};
        if (!parse_required_stroke(item, "stroke", &stem.bits, path, context)) {
            return false;
        }
        if (!iv_stem_stroke_is_unique(phrasing->iv_stems, stem.bits, path, context)) {
            return false;
        }

        if (!parse_iv_stem_tail_allowlist(&stem, item, phrasing->iv_tails, path, context)) {
            destroy_iv_stem_contents(&stem);
            return false;
        }

        const cJSON *forms = required_array(item, "forms", path, context);
        if (forms == NULL || !parse_phrase_form_array(&stem.forms, forms, path, "initial_verbs.forms")) {
            destroy_iv_stem_contents(&stem);
            return false;
        }
        arrput(phrasing->iv_stems, stem);
        ++index;
    }

    return true;
}

static bool parse_fv_starters(Phrasing *phrasing, const cJSON *array, const char *path)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "final_verbs.starters[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Fv_Starter starter = {0};
        if (!parse_required_stroke(item, "stroke", &starter.bits, path, context)) {
            return false;
        }
        if (!fv_starter_stroke_is_unique(phrasing->fv_starters, starter.bits, path, context)) {
            return false;
        }
        if (!copy_required_string(item, "text", &starter.text)) {
            print_field_error(path, context, "text", "must be a string");
            return false;
        }
        char *agreement = NULL;
        if (!copy_required_string(item, "agreement", &agreement) || !parse_agreement(agreement, &starter.agreement)) {
            print_field_error(path, context, "agreement", "must be first_singular, third_singular, or plural");
            free(agreement);
            destroy_fv_starter_contents(&starter);
            return false;
        }
        free(agreement);

        if (!copy_optional_string(item, "be_contraction", &starter.be_contraction)
            || !copy_optional_string(item, "have_contraction", &starter.have_contraction)
            || !copy_optional_string(item, "will_contraction", &starter.will_contraction)) {
            fprintf(stderr, "stoin: phrasing '%s' %s contraction fields must be strings\n", path, context);
            destroy_fv_starter_contents(&starter);
            return false;
        }

        const cJSON *allowlist = cJSON_GetObjectItemCaseSensitive(item, "enders");
        if (allowlist != NULL) {
            starter.has_ender_allowlist = true;
            if (!cJSON_IsArray(allowlist)) {
                print_field_error(path, context, "enders", "must be an array");
                destroy_fv_starter_contents(&starter);
                return false;
            }

            const cJSON *allowed_ender = NULL;
            size_t allowed_index = 0;
            cJSON_ArrayForEach(allowed_ender, allowlist) {
                if (!cJSON_IsString(allowed_ender) || allowed_ender->valuestring == NULL) {
                    fprintf(stderr,
                        "stoin: phrasing '%s' %s.enders[%zu] must be a string\n",
                        path,
                        context,
                        allowed_index);
                    destroy_fv_starter_contents(&starter);
                    return false;
                }

                uint64_t ender_bits = 0;
                if (!parse_stroke_string(allowed_ender->valuestring, &ender_bits)) {
                    fprintf(stderr,
                        "stoin: phrasing '%s' %s.enders[%zu] has invalid outline '%s'\n",
                        path,
                        context,
                        allowed_index,
                        allowed_ender->valuestring);
                    destroy_fv_starter_contents(&starter);
                    return false;
                }

                size_t ender_index = 0;
                if (!find_ender_index(phrasing->fv_enders, ender_bits, &ender_index)) {
                    fprintf(stderr,
                        "stoin: phrasing '%s' %s.enders[%zu] references unknown ender '%s'\n",
                        path,
                        context,
                        allowed_index,
                        allowed_ender->valuestring);
                    destroy_fv_starter_contents(&starter);
                    return false;
                }
                for (size_t i = 0; i < arrlenu(starter.ender_indices); ++i) {
                    if (starter.ender_indices[i] == ender_index) {
                        fprintf(stderr,
                            "stoin: phrasing '%s' %s.enders[%zu] duplicates ender '%s'\n",
                            path,
                            context,
                            allowed_index,
                            allowed_ender->valuestring);
                        destroy_fv_starter_contents(&starter);
                        return false;
                    }
                }
                arrput(starter.ender_indices, ender_index);
                ++allowed_index;
            }
        }

        arrput(phrasing->fv_starters, starter);
        ++index;
    }
    return true;
}

static bool parse_fv_operators(Phrasing *phrasing, const cJSON *array, const char *path)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "final_verbs.operators[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Fv_Operator operator = {0};
        if (!parse_required_stroke(item, "stroke", &operator.bits, path, context)) {
            return false;
        }
        if (!fv_operator_stroke_is_unique(phrasing->fv_operators, operator.bits, path, context)) {
            return false;
        }
        char *modal = NULL;
        if (!copy_required_string(item, "modal", &modal) || !parse_modal(modal, &operator.modal)) {
            print_field_error(path, context, "modal", "must be none, can, should, or will");
            free(modal);
            return false;
        }
        free(modal);
        if (!parse_optional_bool(item, "negative", false, &operator.negative)) {
            print_field_error(path, context, "negative", "must be true or false");
            return false;
        }
        arrput(phrasing->fv_operators, operator);
        ++index;
    }
    return true;
}

static bool parse_fv_structures(Phrasing *phrasing, const cJSON *array, const char *path)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "final_verbs.structures[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Fv_Structure_Row row = {0};
        if (!parse_required_stroke(item, "stroke", &row.bits, path, context)) {
            return false;
        }
        if (!fv_structure_stroke_is_unique(phrasing->fv_structures, row.bits, path, context)) {
            return false;
        }
        char *kind = NULL;
        if (!copy_required_string(item, "kind", &kind) || !parse_structure(kind, &row.structure)) {
            print_field_error(path, context, "kind", "must be simple, progressive, perfect, or perfect_progressive");
            free(kind);
            return false;
        }
        free(kind);
        arrput(phrasing->fv_structures, row);
        ++index;
    }
    return true;
}

static bool parse_fv_verbs(Phrasing *phrasing, const cJSON *array, const char *path)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "final_verbs.verbs[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Fv_Verb verb = {0};
        if (!copy_required_string(item, "id", &verb.id)
            || !copy_required_string(item, "base", &verb.base)
            || !copy_required_string(item, "third", &verb.third)
            || !copy_required_string(item, "past", &verb.past)
            || !copy_required_string(item, "present_participle", &verb.present_participle)
            || !copy_required_string(item, "past_participle", &verb.past_participle)) {
            fprintf(stderr, "stoin: phrasing '%s' %s verb fields must be strings\n", path, context);
            free(verb.id);
            free(verb.base);
            free(verb.third);
            free(verb.past);
            free(verb.present_participle);
            free(verb.past_participle);
            return false;
        }
        verb.kind = verb_kind_for_id(verb.id);
        arrput(phrasing->fv_verbs, verb);
        ++index;
    }
    return true;
}

static bool parse_fv_enders(Phrasing *phrasing, const cJSON *array, const char *path)
{
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "final_verbs.enders[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Fv_Ender ender = {0};
        if (!parse_required_stroke(item, "stroke", &ender.bits, path, context)) {
            return false;
        }
        if (!fv_ender_stroke_is_unique(phrasing->fv_enders, ender.bits, path, context)) {
            return false;
        }
        if (!copy_optional_string(item, "verb", &ender.verb_id)
            || !copy_optional_string(item, "suffix", &ender.suffix)) {
            fprintf(stderr, "stoin: phrasing '%s' %s verb and suffix must be strings or null\n", path, context);
            free(ender.verb_id);
            free(ender.suffix);
            return false;
        }
        if (!parse_optional_bool(item, "past", false, &ender.past)) {
            print_field_error(path, context, "past", "must be true or false");
            free(ender.verb_id);
            free(ender.suffix);
            return false;
        }
        ender.verb = find_verb(phrasing, ender.verb_id);
        if (ender.verb_id != NULL && ender.verb == NULL) {
            fprintf(stderr, "stoin: phrasing '%s' %s references unknown verb '%s'\n", path, context, ender.verb_id);
            free(ender.verb_id);
            free(ender.suffix);
            return false;
        }

        arrput(phrasing->fv_enders, ender);
        ++index;
    }
    return true;
}

static bool parse_final_verbs(Phrasing *phrasing, const cJSON *root, const char *path)
{
    const cJSON *section = required_object(root, "final_verbs", path, "root");
    if (section == NULL) {
        return false;
    }

    if (!parse_required_stroke(section, "contraction_stroke", &phrasing->contraction_bits, path, "final_verbs")) {
        return false;
    }

    const cJSON *starters = required_array(section, "starters", path, "final_verbs");
    const cJSON *operators = required_array(section, "operators", path, "final_verbs");
    const cJSON *structures = required_array(section, "structures", path, "final_verbs");
    const cJSON *verbs = required_array(section, "verbs", path, "final_verbs");
    const cJSON *enders = required_array(section, "enders", path, "final_verbs");
    return starters != NULL && operators != NULL && structures != NULL && verbs != NULL && enders != NULL
        && parse_fv_operators(phrasing, operators, path)
        && parse_fv_structures(phrasing, structures, path)
        && parse_fv_verbs(phrasing, verbs, path)
        && parse_fv_enders(phrasing, enders, path)
        && parse_fv_starters(phrasing, starters, path);
}

Phrasing *phrasing_load(const char *path)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read phrasing '%s'\n", path);
        return NULL;
    }

    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(file, size, &parse_end, 0);
    if (root == NULL) {
        print_json_parse_error(path, file, parse_end);
        free(file);
        return NULL;
    }
    if (!cJSON_IsObject(root)) {
        fprintf(stderr, "stoin: phrasing '%s' is not a JSON object\n", path);
        cJSON_Delete(root);
        free(file);
        return NULL;
    }

    Phrasing *phrasing = calloc(1, sizeof(*phrasing));
    if (phrasing == NULL) {
        cJSON_Delete(root);
        free(file);
        return NULL;
    }

    const bool ok = parse_initial_verbs(phrasing, root, path)
        && parse_final_verbs(phrasing, root, path);
    cJSON_Delete(root);
    free(file);
    if (!ok) {
        phrasing_destroy(phrasing);
        return NULL;
    }
    return phrasing;
}

void phrasing_destroy(Phrasing *phrasing)
{
    if (phrasing == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(phrasing->iv_stems); ++i) {
        destroy_iv_stem_contents(&phrasing->iv_stems[i]);
    }
    arrfree(phrasing->iv_stems);
    for (size_t i = 0; i < arrlenu(phrasing->iv_tails); ++i) {
        free(phrasing->iv_tails[i].id);
        free(phrasing->iv_tails[i].text);
    }
    arrfree(phrasing->iv_tails);
    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        destroy_fv_starter_contents(&phrasing->fv_starters[i]);
    }
    arrfree(phrasing->fv_starters);
    arrfree(phrasing->fv_operators);
    arrfree(phrasing->fv_structures);
    for (size_t i = 0; i < arrlenu(phrasing->fv_verbs); ++i) {
        free(phrasing->fv_verbs[i].id);
        free(phrasing->fv_verbs[i].base);
        free(phrasing->fv_verbs[i].third);
        free(phrasing->fv_verbs[i].past);
        free(phrasing->fv_verbs[i].present_participle);
        free(phrasing->fv_verbs[i].past_participle);
    }
    arrfree(phrasing->fv_verbs);
    for (size_t i = 0; i < arrlenu(phrasing->fv_enders); ++i) {
        free(phrasing->fv_enders[i].verb_id);
        free(phrasing->fv_enders[i].suffix);
    }
    arrfree(phrasing->fv_enders);
    free(phrasing);
}

static Phrase_Lookup_Result lookup_initial_verb(const Phrasing *phrasing, uint64_t bits, char **out_utf8)
{
    for (size_t i = 0; i < arrlenu(phrasing->iv_stems); ++i) {
        const Iv_Stem *stem = &phrasing->iv_stems[i];
        for (size_t j = 0; j < arrlenu(stem->forms); ++j) {
            const Phrase_Form *form = &stem->forms[j];
            for (size_t k = 0; k < arrlenu(phrasing->iv_tails); ++k) {
                const Phrase_Tail *tail = &phrasing->iv_tails[k];
                if (stem->has_tail_allowlist) {
                    bool allowed = false;
                    for (size_t m = 0; m < arrlenu(stem->tail_indices); ++m) {
                        if (stem->tail_indices[m] == k) {
                            allowed = true;
                            break;
                        }
                    }
                    if (!allowed) {
                        continue;
                    }
                }
                if (bits == (stem->bits | form->bits | tail->bits)) {
                    return copy_phrase_words(form->text, tail->text, out_utf8);
                }
            }
        }
    }
    return PHRASE_LOOKUP_MISS;
}

static const char *fv_be_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return starter->agreement == FV_AGREEMENT_PLURAL ? "were" : "was";
    }
    if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR) {
        return "am";
    }
    return starter->agreement == FV_AGREEMENT_PLURAL ? "are" : "is";
}

static const char *fv_have_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "had";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "has" : "have";
}

static const char *fv_do_word(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "did";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "does" : "do";
}

static const char *fv_finite_verb_word(const Fv_Starter *starter, const Fv_Verb *verb, bool past)
{
    if (verb->kind == FV_VERB_BE) {
        return fv_be_word(starter, past);
    }
    if (verb->kind == FV_VERB_HAVE) {
        return fv_have_word(starter, past);
    }
    if (verb->kind == FV_VERB_DO) {
        return fv_do_word(starter, past);
    }
    if (past) {
        return verb->past;
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? verb->third : verb->base;
}

static const char *fv_modal_word(Fv_Modal modal, bool past, bool negative)
{
    switch (modal) {
    case FV_MODAL_CAN:
        if (negative) {
            return past ? "could not" : "cannot";
        }
        return past ? "could" : "can";
    case FV_MODAL_SHOULD:
        return negative ? "should not" : "should";
    case FV_MODAL_WILL:
        if (negative) {
            return past ? "would not" : "will not";
        }
        return past ? "would" : "will";
    case FV_MODAL_NONE:
    default:
        return "";
    }
}

static const char *fv_modal_negative_contraction(Fv_Modal modal, bool past)
{
    switch (modal) {
    case FV_MODAL_CAN:
        return past ? "couldn't" : "can't";
    case FV_MODAL_SHOULD:
        return "shouldn't";
    case FV_MODAL_WILL:
        return past ? "wouldn't" : "won't";
    case FV_MODAL_NONE:
    default:
        return NULL;
    }
}

static const char *fv_be_negative_contraction(const Fv_Starter *starter, bool past)
{
    if (past) {
        return starter->agreement == FV_AGREEMENT_PLURAL ? "weren't" : "wasn't";
    }
    if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR) {
        return NULL;
    }
    return starter->agreement == FV_AGREEMENT_PLURAL ? "aren't" : "isn't";
}

static const char *fv_have_negative_contraction(const Fv_Starter *starter, bool past)
{
    if (past) {
        return "hadn't";
    }
    return starter->agreement == FV_AGREEMENT_THIRD_SINGULAR ? "hasn't" : "haven't";
}

static bool append_verb_and_suffix(char **text, const char *verb, const char *suffix)
{
    return append_word(text, verb) && append_word(text, suffix);
}

static bool append_modal_complement(char **text, Fv_Structure structure, const Fv_Ender *ender)
{
    const bool has_verb = ender->verb != NULL;
    switch (structure) {
    case FV_STRUCTURE_SIMPLE:
        return !has_verb || append_verb_and_suffix(text, ender->verb->base, ender->suffix);
    case FV_STRUCTURE_PROGRESSIVE:
        return append_word(text, "be")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT:
        return append_word(text, "have")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->past_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT_PROGRESSIVE:
        return append_word(text, "have")
            && append_word(text, "been")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    default:
        return false;
    }
}

static bool build_fv_long(
    const Fv_Starter *starter,
    Fv_Operator operator,
    Fv_Structure structure,
    const Fv_Ender *ender,
    char **out
)
{
    const bool has_verb = ender->verb != NULL;
    if (operator.modal == FV_MODAL_NONE && structure == FV_STRUCTURE_SIMPLE && !has_verb) {
        return false;
    }

    if (!append_word(out, starter->text)) {
        return false;
    }

    if (operator.modal != FV_MODAL_NONE) {
        return append_word(out, fv_modal_word(operator.modal, ender->past, operator.negative))
            && append_modal_complement(out, structure, ender);
    }

    switch (structure) {
    case FV_STRUCTURE_SIMPLE:
        if (operator.negative && ender->verb->kind != FV_VERB_BE) {
            return append_word(out, fv_do_word(starter, ender->past))
                && append_word(out, "not")
                && append_verb_and_suffix(out, ender->verb->base, ender->suffix);
        }
        return append_word(out, fv_finite_verb_word(starter, ender->verb, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && append_word(out, ender->suffix);
    case FV_STRUCTURE_PROGRESSIVE:
        return append_word(out, fv_be_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && (!has_verb || append_verb_and_suffix(out, ender->verb->present_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT:
        return append_word(out, fv_have_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && (!has_verb || append_verb_and_suffix(out, ender->verb->past_participle, ender->suffix));
    case FV_STRUCTURE_PERFECT_PROGRESSIVE:
        return append_word(out, fv_have_word(starter, ender->past))
            && (!operator.negative || append_word(out, "not"))
            && append_word(out, "been")
            && (!has_verb || append_verb_and_suffix(out, ender->verb->present_participle, ender->suffix));
    default:
        return false;
    }
}

static bool append_be_contraction_complement(
    char **text,
    Fv_Structure structure,
    const Fv_Ender *ender
)
{
    const bool has_verb = ender->verb != NULL;
    if (structure == FV_STRUCTURE_SIMPLE && ender->verb != NULL && ender->verb->kind == FV_VERB_BE) {
        return append_word(text, ender->suffix);
    }
    if (structure == FV_STRUCTURE_PROGRESSIVE) {
        return !has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix);
    }
    return false;
}

static bool append_have_contraction_complement(
    char **text,
    Fv_Structure structure,
    const Fv_Ender *ender
)
{
    const bool has_verb = ender->verb != NULL;
    if (structure == FV_STRUCTURE_PERFECT) {
        return !has_verb || append_verb_and_suffix(text, ender->verb->past_participle, ender->suffix);
    }
    if (structure == FV_STRUCTURE_PERFECT_PROGRESSIVE) {
        return append_word(text, "been")
            && (!has_verb || append_verb_and_suffix(text, ender->verb->present_participle, ender->suffix));
    }
    return false;
}

static bool build_fv_contraction(
    const Fv_Starter *starter,
    Fv_Operator operator,
    Fv_Structure structure,
    const Fv_Ender *ender,
    char **out
)
{
    if (operator.modal != FV_MODAL_NONE) {
        if (operator.negative) {
            const char *modal = fv_modal_negative_contraction(operator.modal, ender->past);
            return modal != NULL
                && append_word(out, starter->text)
                && append_word(out, modal)
                && append_modal_complement(out, structure, ender);
        }
        if (operator.modal == FV_MODAL_WILL && !ender->past) {
            return starter->will_contraction != NULL
                && append_word(out, starter->will_contraction)
                && append_modal_complement(out, structure, ender);
        }
        return false;
    }

    if (operator.negative
        && (structure == FV_STRUCTURE_PROGRESSIVE
            || (structure == FV_STRUCTURE_SIMPLE && ender->verb != NULL && ender->verb->kind == FV_VERB_BE))) {
        if (starter->agreement == FV_AGREEMENT_FIRST_SINGULAR && !ender->past) {
            return starter->be_contraction != NULL
                && append_word(out, starter->be_contraction)
                && append_word(out, "not")
                && append_be_contraction_complement(out, structure, ender);
        }
        const char *negative = fv_be_negative_contraction(starter, ender->past);
        return negative != NULL
            && append_word(out, starter->text)
            && append_word(out, negative)
            && append_be_contraction_complement(out, structure, ender);
    }

    if (!operator.negative
        && !ender->past
        && (structure == FV_STRUCTURE_PROGRESSIVE
            || (structure == FV_STRUCTURE_SIMPLE && ender->verb != NULL && ender->verb->kind == FV_VERB_BE))) {
        return starter->be_contraction != NULL
            && append_word(out, starter->be_contraction)
            && append_be_contraction_complement(out, structure, ender);
    }

    if (structure == FV_STRUCTURE_PERFECT || structure == FV_STRUCTURE_PERFECT_PROGRESSIVE) {
        if (operator.negative) {
            const char *negative = fv_have_negative_contraction(starter, ender->past);
            return negative != NULL
                && append_word(out, starter->text)
                && append_word(out, negative)
                && append_have_contraction_complement(out, structure, ender);
        }
        if (!ender->past) {
            return starter->have_contraction != NULL
                && append_word(out, starter->have_contraction)
                && append_have_contraction_complement(out, structure, ender);
        }
    }

    return false;
}

static Phrase_Lookup_Result lookup_final_verb(const Phrasing *phrasing, uint64_t bits, char **out_utf8)
{
    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        const Fv_Starter *starter = &phrasing->fv_starters[i];
        for (size_t j = 0; j < arrlenu(phrasing->fv_operators); ++j) {
            Fv_Operator operator = phrasing->fv_operators[j];
            for (size_t k = 0; k < arrlenu(phrasing->fv_structures); ++k) {
                const Fv_Structure_Row *structure = &phrasing->fv_structures[k];
                for (size_t m = 0; m < arrlenu(phrasing->fv_enders); ++m) {
                    const Fv_Ender *ender = &phrasing->fv_enders[m];
                    if (starter->has_ender_allowlist) {
                        bool allowed = false;
                        for (size_t n = 0; n < arrlenu(starter->ender_indices); ++n) {
                            if (starter->ender_indices[n] == m) {
                                allowed = true;
                                break;
                            }
                        }
                        if (!allowed) {
                            continue;
                        }
                    }
                    const uint64_t long_bits = starter->bits | operator.bits | structure->bits | ender->bits;
                    const bool contraction = bits == (long_bits | phrasing->contraction_bits);
                    if (bits != long_bits && !contraction) {
                        continue;
                    }

                    char *text = NULL;
                    const bool ok = contraction
                        ? build_fv_contraction(starter, operator, structure->structure, ender, &text)
                        : build_fv_long(starter, operator, structure->structure, ender, &text);
                    if (!ok || text == NULL || text[0] == '\0') {
                        arrfree(text);
                        continue;
                    }

                    *out_utf8 = copy_cstring(text);
                    arrfree(text);
                    return *out_utf8 == NULL ? PHRASE_LOOKUP_ERROR : PHRASE_LOOKUP_HIT;
                }
            }
        }
    }
    return PHRASE_LOOKUP_MISS;
}

Phrase_Lookup_Result phrasing_lookup(
    const Phrasing *phrasing,
    uint64_t stroke_bits,
    char **out_utf8
)
{
    if (out_utf8 == NULL) {
        return PHRASE_LOOKUP_ERROR;
    }
    *out_utf8 = NULL;
    if (phrasing == NULL) {
        return PHRASE_LOOKUP_MISS;
    }

    Phrase_Lookup_Result result = lookup_initial_verb(phrasing, stroke_bits, out_utf8);
    return result != PHRASE_LOOKUP_MISS
        ? result
        : lookup_final_verb(phrasing, stroke_bits, out_utf8);
}
