#include "phrasing_internal.h"

#include "steno_stroke.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/cjson/cJSON.h"
#include "../third_party/stb_ds.h"

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

static bool nv_prefix_stroke_is_unique(
    const Nv_Prefix *prefixes,
    uint64_t bits,
    const char *path,
    const char *context
)
{
    for (size_t i = 0; i < arrlenu(prefixes); ++i) {
        if (prefixes[i].bits == bits) {
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

static bool parse_optional_stroke_allowlist(
    const cJSON *parent,
    const char *field,
    uint64_t **out_bits,
    bool *out_has_allowlist,
    const char *path,
    const char *context
)
{
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (array == NULL) {
        return true;
    }

    *out_has_allowlist = true;
    if (!cJSON_IsArray(array)) {
        print_field_error(path, context, field, "must be an array");
        return false;
    }

    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            fprintf(stderr,
                "stoin: phrasing '%s' %s.%s[%zu] must be a stroke string\n",
                path,
                context,
                field,
                index);
            return false;
        }

        uint64_t bits = 0;
        if (!parse_stroke_string(item->valuestring, &bits)) {
            fprintf(stderr,
                "stoin: phrasing '%s' %s.%s[%zu] has invalid outline '%s'\n",
                path,
                context,
                field,
                index,
                item->valuestring);
            return false;
        }
        for (size_t i = 0; i < arrlenu(*out_bits); ++i) {
            if ((*out_bits)[i] == bits) {
                fprintf(stderr,
                    "stoin: phrasing '%s' %s.%s[%zu] duplicates outline '%s'\n",
                    path,
                    context,
                    field,
                    index,
                    item->valuestring[0] != '\0' ? item->valuestring : "<empty>");
                return false;
            }
        }
        arrput(*out_bits, bits);
        ++index;
    }
    return true;
}

static void destroy_phrase_tail_contents(Phrase_Tail *tail)
{
    if (tail == NULL) {
        return;
    }
    free(tail->id);
    free(tail->text);
    arrfree(tail->stem_bits);
    arrfree(tail->form_bits);
}

static bool parse_tail_array(
    Phrase_Tail **out_tails,
    const cJSON *array,
    bool parse_iv_allowlists,
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
        if (parse_iv_allowlists
            && (!parse_optional_stroke_allowlist(
                    item,
                    "stems",
                    &tail.stem_bits,
                    &tail.has_stem_allowlist,
                    path,
                    item_context)
                || !parse_optional_stroke_allowlist(
                    item,
                    "forms",
                    &tail.form_bits,
                    &tail.has_form_allowlist,
                    path,
                    item_context))) {
            destroy_phrase_tail_contents(&tail);
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

static void destroy_nv_prefix_contents(Nv_Prefix *prefix)
{
    if (prefix == NULL) {
        return;
    }
    free(prefix->text);
    arrfree(prefix->tail_indices);
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
    free(starter->d_contraction);
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
    if (tails == NULL || stems == NULL
        || !parse_tail_array(&phrasing->iv_tails, tails, true, path, "initial_verbs.tails")) {
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

static bool parse_nonverbs(Phrasing *phrasing, const cJSON *root, const char *path)
{
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(root, "nonverbs");
    if (section == NULL) {
        return true;
    }
    if (!cJSON_IsObject(section)) {
        print_field_error(path, "root", "nonverbs", "must be an object");
        return false;
    }

    const cJSON *tails = required_array(section, "tails", path, "nonverbs");
    const cJSON *prefixes = required_array(section, "prefixes", path, "nonverbs");
    if (tails == NULL || prefixes == NULL
        || !parse_tail_array(&phrasing->nv_tails, tails, false, path, "nonverbs.tails")) {
        return false;
    }

    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, prefixes) {
        char context[128] = {0};
        snprintf(context, sizeof(context), "nonverbs.prefixes[%zu]", index);
        if (!cJSON_IsObject(item)) {
            fprintf(stderr, "stoin: phrasing '%s' %s must be an object\n", path, context);
            return false;
        }

        Nv_Prefix prefix = {0};
        if (!parse_required_stroke(item, "stroke", &prefix.bits, path, context)
            || !nv_prefix_stroke_is_unique(phrasing->nv_prefixes, prefix.bits, path, context)) {
            return false;
        }
        if (!copy_required_string(item, "text", &prefix.text)) {
            print_field_error(path, context, "text", "must be a string");
            return false;
        }

        const cJSON *tail_ids = required_array(item, "tails", path, context);
        if (tail_ids == NULL) {
            destroy_nv_prefix_contents(&prefix);
            return false;
        }
        const cJSON *tail_id = NULL;
        size_t tail_id_index = 0;
        cJSON_ArrayForEach(tail_id, tail_ids) {
            if (!cJSON_IsString(tail_id) || tail_id->valuestring == NULL) {
                fprintf(stderr,
                    "stoin: phrasing '%s' %s.tails[%zu] must be a string\n",
                    path,
                    context,
                    tail_id_index);
                destroy_nv_prefix_contents(&prefix);
                return false;
            }

            size_t tail_index = 0;
            if (!find_tail_index(phrasing->nv_tails, tail_id->valuestring, &tail_index)) {
                fprintf(stderr,
                    "stoin: phrasing '%s' %s.tails[%zu] references unknown tail id '%s'\n",
                    path,
                    context,
                    tail_id_index,
                    tail_id->valuestring);
                destroy_nv_prefix_contents(&prefix);
                return false;
            }
            for (size_t i = 0; i < arrlenu(prefix.tail_indices); ++i) {
                if (prefix.tail_indices[i] == tail_index) {
                    fprintf(stderr,
                        "stoin: phrasing '%s' %s.tails[%zu] duplicates tail id '%s'\n",
                        path,
                        context,
                        tail_id_index,
                        tail_id->valuestring);
                    destroy_nv_prefix_contents(&prefix);
                    return false;
                }
            }
            arrput(prefix.tail_indices, tail_index);
            ++tail_id_index;
        }
        arrput(phrasing->nv_prefixes, prefix);
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
            || !copy_optional_string(item, "will_contraction", &starter.will_contraction)
            || !copy_optional_string(item, "d_contraction", &starter.d_contraction)) {
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

static bool validate_phrase_fragment_overlaps(const Phrasing *phrasing, const char *path)
{
    for (size_t i = 0; i < arrlenu(phrasing->iv_stems); ++i) {
        const Iv_Stem *stem = &phrasing->iv_stems[i];
        for (size_t j = 0; j < arrlenu(stem->forms); ++j) {
            const Phrase_Form *form = &stem->forms[j];
            if ((stem->bits & form->bits) != 0) {
                fprintf(stderr,
                    "stoin: phrasing '%s' initial_verbs stem %zu and form %zu overlap\n",
                    path,
                    i,
                    j);
                return false;
            }
            for (size_t k = 0; k < arrlenu(phrasing->iv_tails); ++k) {
                const Phrase_Tail *tail = &phrasing->iv_tails[k];
                if (iv_combination_is_allowed(stem, k, tail, form)
                    && (((stem->bits | form->bits) & tail->bits) != 0)) {
                    fprintf(stderr,
                        "stoin: phrasing '%s' initial_verbs stem %zu, form %zu, and tail %zu overlap\n",
                        path,
                        i,
                        j,
                        k);
                    return false;
                }
            }
        }
    }

    for (size_t i = 0; i < arrlenu(phrasing->nv_prefixes); ++i) {
        const Nv_Prefix *prefix = &phrasing->nv_prefixes[i];
        for (size_t j = 0; j < arrlenu(prefix->tail_indices); ++j) {
            const size_t tail_index = prefix->tail_indices[j];
            if ((prefix->bits & phrasing->nv_tails[tail_index].bits) != 0) {
                fprintf(stderr,
                    "stoin: phrasing '%s' nonverbs prefix %zu and tail %zu overlap\n",
                    path,
                    i,
                    tail_index);
                return false;
            }
        }
    }

    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        const Fv_Starter *starter = &phrasing->fv_starters[i];
        for (size_t j = 0; j < arrlenu(phrasing->fv_operators); ++j) {
            const Fv_Operator *operator = &phrasing->fv_operators[j];
            for (size_t k = 0; k < arrlenu(phrasing->fv_structures); ++k) {
                const Fv_Structure_Row *structure = &phrasing->fv_structures[k];
                for (size_t m = 0; m < arrlenu(phrasing->fv_enders); ++m) {
                    if (!fv_starter_allows_ender(starter, m)) {
                        continue;
                    }
                    const Fv_Ender *ender = &phrasing->fv_enders[m];
                    const uint64_t fragments[] = {
                        starter->bits,
                        operator->bits,
                        structure->bits,
                        ender->bits,
                        phrasing->contraction_bits,
                    };
                    for (size_t first = 0; first < 5; ++first) {
                        for (size_t second = first + 1; second < 5; ++second) {
                            if ((fragments[first] & fragments[second]) != 0) {
                                fprintf(stderr,
                                    "stoin: phrasing '%s' final_verbs starter %zu, operator %zu, structure %zu, and ender %zu overlap\n",
                                    path,
                                    i,
                                    j,
                                    k,
                                    m);
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

static bool validate_shared_phrase_banks(const Phrasing *phrasing, const char *path)
{
    if (arrlenu(phrasing->iv_tails) != arrlenu(phrasing->nv_tails)) {
        fprintf(stderr,
            "stoin: phrasing '%s' shared IV/NV tail banks must have the same length\n",
            path);
        return false;
    }
    for (size_t i = 0; i < arrlenu(phrasing->iv_tails); ++i) {
        const Phrase_Tail *iv = &phrasing->iv_tails[i];
        const Phrase_Tail *nv = &phrasing->nv_tails[i];
        if (iv->bits != nv->bits
            || strcmp(iv->id, nv->id) != 0
            || strcmp(iv->text, nv->text) != 0) {
            fprintf(stderr,
                "stoin: phrasing '%s' shared IV/NV tail banks differ at index %zu\n",
                path,
                i);
            return false;
        }
    }

    if (arrlenu(phrasing->fv_starters) != arrlenu(phrasing->nv_prefixes)) {
        fprintf(stderr,
            "stoin: phrasing '%s' shared FV/NV starter banks must have the same length\n",
            path);
        return false;
    }
    for (size_t i = 0; i < arrlenu(phrasing->fv_starters); ++i) {
        const Fv_Starter *fv = &phrasing->fv_starters[i];
        const Nv_Prefix *nv = &phrasing->nv_prefixes[i];
        if (fv->bits != nv->bits || strcmp(fv->text, nv->text) != 0) {
            fprintf(stderr,
                "stoin: phrasing '%s' shared FV/NV starter banks differ at index %zu\n",
                path,
                i);
            return false;
        }
        if (arrlenu(nv->tail_indices) != arrlenu(phrasing->nv_tails)) {
            fprintf(stderr,
                "stoin: phrasing '%s' shared NV prefix at index %zu must enable every tail\n",
                path,
                i);
            return false;
        }
        for (size_t j = 0; j < arrlenu(nv->tail_indices); ++j) {
            if (nv->tail_indices[j] != j) {
                fprintf(stderr,
                    "stoin: phrasing '%s' shared NV prefix at index %zu must list tails in shared-bank order\n",
                    path,
                    i);
                return false;
            }
        }
    }
    return true;
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

    bool shared_banks = false;
    const bool ok = parse_optional_bool(root, "shared_banks", false, &shared_banks)
        && parse_initial_verbs(phrasing, root, path)
        && parse_nonverbs(phrasing, root, path)
        && parse_final_verbs(phrasing, root, path)
        && validate_phrase_fragment_overlaps(phrasing, path)
        && (!shared_banks || validate_shared_phrase_banks(phrasing, path));
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
        destroy_phrase_tail_contents(&phrasing->iv_tails[i]);
    }
    arrfree(phrasing->iv_tails);
    for (size_t i = 0; i < arrlenu(phrasing->nv_prefixes); ++i) {
        destroy_nv_prefix_contents(&phrasing->nv_prefixes[i]);
    }
    arrfree(phrasing->nv_prefixes);
    for (size_t i = 0; i < arrlenu(phrasing->nv_tails); ++i) {
        destroy_phrase_tail_contents(&phrasing->nv_tails[i]);
    }
    arrfree(phrasing->nv_tails);
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
    shfree(phrasing->suggestions);
    free(phrasing);
}
