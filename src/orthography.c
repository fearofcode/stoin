#include "orthography.h"

#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

typedef bool (*Orthography_Rule_Fn)(const char *word, const char *suffix, char *out, size_t out_size);

typedef struct Orthography_Rule {
    const char *name;
    Orthography_Rule_Fn apply;
} Orthography_Rule;

static bool starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool ends_with(const char *s, const char *suffix)
{
    const size_t s_len = strlen(s);
    const size_t suffix_len = strlen(suffix);
    return s_len >= suffix_len && memcmp(s + s_len - suffix_len, suffix, suffix_len) == 0;
}

static bool string_equals(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static bool char_in(char c, const char *set)
{
    return strchr(set, c) != NULL;
}

static bool is_consonant(char c)
{
    return char_in(c, "bcdfghjklmnpqrstvwxz");
}

static bool copy_prefix_suffix(
    const char *word,
    size_t prefix_len,
    const char *suffix,
    char *out,
    size_t out_size
)
{
    const size_t suffix_len = strlen(suffix);
    if (prefix_len + suffix_len + 1 > out_size) {
        return false;
    }

    memcpy(out, word, prefix_len);
    memcpy(out + prefix_len, suffix, suffix_len);
    out[prefix_len + suffix_len] = '\0';
    return true;
}

static bool write_parts(
    char *out,
    size_t out_size,
    const char *a,
    size_t a_len,
    const char *b,
    size_t b_len,
    const char *c,
    size_t c_len
)
{
    if (a_len + b_len + c_len + 1 > out_size) {
        return false;
    }
    memcpy(out, a, a_len);
    memcpy(out + a_len, b, b_len);
    memcpy(out + a_len + b_len, c, c_len);
    out[a_len + b_len + c_len] = '\0';
    return true;
}

static bool has_suffix_from(const char *s, const char *const *suffixes, size_t suffix_count)
{
    for (size_t i = 0; i < suffix_count; ++i) {
        if (ends_with(s, suffixes[i])) {
            return true;
        }
    }
    return false;
}

static bool rule_ic_ally(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "c") || !string_equals(suffix, "ly") || word_len < 3) {
        return false;
    }

    const char c = word[word_len - 2];
    if (!char_in(c, "aeiou")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len, "ally", out, out_size);
}

static bool rule_le_ly(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "le") || !string_equals(suffix, "ly") || word_len < 3) {
        return false;
    }
    if (!char_in(word[word_len - 3], "aeioubmnp")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 2, "ly", out, out_size);
}

static bool rule_te_ory(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "te") || (!string_equals(suffix, "ry") && !string_equals(suffix, "ary"))) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, "ory", out, out_size);
}

static bool rule_m_atory(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "m") || !starts_with(suffix, "tor")) {
        return false;
    }

    if (ends_with(suffix, "ily")) {
        return copy_prefix_suffix(word, word_len - 1, "matorily", out, out_size);
    }
    if (ends_with(suffix, "y")) {
        return copy_prefix_suffix(word, word_len - 1, "matory", out, out_size);
    }
    return false;
}

static bool rule_se_ary(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "se") || word_len < 3 || !starts_with(suffix, "ar")) {
        return false;
    }

    if (ends_with(suffix, "y")) {
        return copy_prefix_suffix(word, word_len - 2, "sory", out, out_size);
    }
    if (ends_with(suffix, "ies")) {
        return copy_prefix_suffix(word, word_len - 2, "sories", out, out_size);
    }
    return false;
}

static bool rule_t_cy(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!string_equals(suffix, "cy")) {
        return false;
    }

    const char *const te_suffixes[] = { "nte", "ate", "ete", "ite", "ote", "ute" };
    const char *const t_suffixes[] = { "nt", "at", "et", "it", "ot", "ut" };
    if (has_suffix_from(word, te_suffixes, sizeof(te_suffixes) / sizeof(te_suffixes[0]))) {
        return copy_prefix_suffix(word, word_len - 2, "cy", out, out_size);
    }
    if (has_suffix_from(word, t_suffixes, sizeof(t_suffixes) / sizeof(t_suffixes[0]))) {
        return copy_prefix_suffix(word, word_len - 1, "cy", out, out_size);
    }
    return false;
}

static bool rule_sibilant_plural(const char *word, const char *suffix, char *out, size_t out_size)
{
    if (!string_equals(suffix, "s")) {
        return false;
    }
    if (ends_with(word, "s") || ends_with(word, "sh") || ends_with(word, "x")
        || ends_with(word, "z") || ends_with(word, "zh")) {
        return copy_prefix_suffix(word, strlen(word), "es", out, out_size);
    }
    return false;
}

static bool rule_ch_plural(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!string_equals(suffix, "s") || !ends_with(word, "ch") || word_len < 3) {
        return false;
    }

    const char before_ch = word[word_len - 3];
    bool has_vowel_pair = false;
    if (word_len >= 4) {
        const char *two_before = word + word_len - 4;
        has_vowel_pair =
            memcmp(two_before, "oa", 2) == 0 ||
            memcmp(two_before, "ea", 2) == 0 ||
            memcmp(two_before, "ee", 2) == 0 ||
            memcmp(two_before, "oo", 2) == 0 ||
            memcmp(two_before, "au", 2) == 0 ||
            memcmp(two_before, "ou", 2) == 0;
    }

    const bool valid_single = before_ch == 'i' || before_ch == 'l' || before_ch == 'n' || before_ch == 't';
    bool valid_r = false;
    if (before_ch == 'r') {
        if (word_len < 5) {
            valid_r = true;
        } else {
            const char *before_r = word + word_len - 5;
            valid_r =
                memcmp(before_r, "ga", 2) != 0 &&
                memcmp(before_r, "ia", 2) != 0 &&
                memcmp(before_r, "na", 2) != 0;
        }
    }

    if (!has_vowel_pair && !valid_single && !valid_r) {
        return false;
    }
    return copy_prefix_suffix(word, word_len, "es", out, out_size);
}

static bool rule_consonant_y_plural(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!string_equals(suffix, "s") || word_len < 3 || !ends_with(word, "y")) {
        return false;
    }
    if (!is_consonant(word[word_len - 2])) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, "ies", out, out_size);
}

static bool rule_ying(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ie") || word_len < 3 || !string_equals(suffix, "ing")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 2, "ying", out, out_size);
}

static bool rule_ie_ed(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ie") || word_len < 3 || !string_equals(suffix, "ed")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len, "d", out, out_size);
}

static bool rule_yist(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "y") || word_len < 4 || !string_equals(suffix, "ist")) {
        return false;
    }
    if (!char_in(word[word_len - 2], "cdfghlmnpr")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, "ist", out, out_size);
}

static bool rule_y_to_i(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "y") || word_len < 3 || suffix[0] == '\0') {
        return false;
    }
    if (!is_consonant(word[word_len - 2]) || suffix[0] == 'i' || suffix[0] == 'y') {
        return false;
    }
    return write_parts(out, out_size, word, word_len - 1, "i", 1, suffix, strlen(suffix));
}

static bool rule_tten(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "te") || word_len < 3 || !string_equals(suffix, "en")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 2, "tten", out, out_size);
}

static bool rule_ae_en(const char *word, const char *suffix, char *out, size_t out_size)
{
    if ((!ends_with(word, "e") && !ends_with(word, "a"))
        || strlen(word) < 3
        || strlen(suffix) < 2
        || suffix[0] != 'e') {
        return false;
    }
    if (ends_with(suffix, "ns")) {
        return copy_prefix_suffix(word, strlen(word), "ns", out, out_size);
    }
    if (ends_with(suffix, "n")) {
        return copy_prefix_suffix(word, strlen(word), "n", out, out_size);
    }
    return false;
}

static bool rule_yial(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "y") || word_len < 3
        || (!string_equals(suffix, "ial") && !string_equals(suffix, "ially"))) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool suffix_is_if_form(const char *suffix)
{
    return starts_with(suffix, "if")
        && (ends_with(suffix, "y")
            || ends_with(suffix, "ying")
            || ends_with(suffix, "ied")
            || ends_with(suffix, "ies")
            || ends_with(suffix, "ication")
            || ends_with(suffix, "ications"));
}

static bool rule_ification(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "i") || word_len < 3 || !suffix_is_if_form(suffix)) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_yify(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if ((!ends_with(word, "y") && !ends_with(word, "i")) || word_len < 3 || !suffix_is_if_form(suffix)) {
        return false;
    }
    if (strlen(suffix) < 2) {
        return false;
    }
    return write_parts(out, out_size, word, word_len - 1, "if", 2, suffix + 2, strlen(suffix + 2));
}

static bool rule_ical(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ic") || word_len < 3
        || (!string_equals(suffix, "ical") && !string_equals(suffix, "ically"))) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 2, suffix, out, out_size);
}

static bool rule_ological(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ology") || word_len < 6
        || (!string_equals(suffix, "ical") && !string_equals(suffix, "ically"))) {
        return false;
    }
    if (string_equals(suffix, "ical")) {
        return copy_prefix_suffix(word, word_len - 5, "ological", out, out_size);
    }
    return copy_prefix_suffix(word, word_len - 5, "ologically", out, out_size);
}

static bool rule_yical(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ry") || word_len < 3
        || (!string_equals(suffix, "ical")
            && !string_equals(suffix, "ically")
            && !string_equals(suffix, "icity"))) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_alist(const char *word, const char *suffix, char *out, size_t out_size)
{
    if (!ends_with(word, "l") || strlen(word) < 3
        || (!string_equals(suffix, "ist") && !string_equals(suffix, "ists"))) {
        return false;
    }
    return copy_prefix_suffix(word, strlen(word), suffix, out, out_size);
}

static bool rule_arity(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ry") || word_len < 2 || !string_equals(suffix, "ity")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_ality(const char *word, const char *suffix, char *out, size_t out_size)
{
    if (!ends_with(word, "l") || strlen(word) < 2 || !string_equals(suffix, "ity")) {
        return false;
    }
    return copy_prefix_suffix(word, strlen(word), suffix, out, out_size);
}

static bool suffix_is_tive_form(const char *suffix)
{
    return starts_with(suffix, "tiv")
        && (ends_with(suffix, "e") || ends_with(suffix, "ity") || ends_with(suffix, "ities"));
}

static bool rule_mative(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "rm") || word_len < 3 || !suffix_is_tive_form(suffix)) {
        return false;
    }
    if (ends_with(suffix, "ities")) {
        return copy_prefix_suffix(word, word_len - 1, "mativities", out, out_size);
    }
    if (ends_with(suffix, "ity")) {
        return copy_prefix_suffix(word, word_len - 1, "mativity", out, out_size);
    }
    return copy_prefix_suffix(word, word_len - 1, "mative", out, out_size);
}

static bool rule_eative(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "e") || word_len < 3 || !suffix_is_tive_form(suffix)) {
        return false;
    }
    if (ends_with(suffix, "ities")) {
        return copy_prefix_suffix(word, word_len - 1, "ativities", out, out_size);
    }
    if (ends_with(suffix, "ity")) {
        return copy_prefix_suffix(word, word_len - 1, "ativity", out, out_size);
    }
    return copy_prefix_suffix(word, word_len - 1, "ative", out, out_size);
}

static bool suffix_is_ize_form(const char *suffix)
{
    return (starts_with(suffix, "iz") || starts_with(suffix, "is"))
        && (ends_with(suffix, "e")
            || ends_with(suffix, "es")
            || ends_with(suffix, "ing")
            || ends_with(suffix, "ed")
            || ends_with(suffix, "er")
            || ends_with(suffix, "ers")
            || ends_with(suffix, "ation")
            || ends_with(suffix, "ations")
            || ends_with(suffix, "m")
            || ends_with(suffix, "ms")
            || ends_with(suffix, "able")
            || ends_with(suffix, "ability")
            || ends_with(suffix, "abilities"));
}

static bool rule_ize_ise(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "y") || word_len < 2 || !suffix_is_ize_form(suffix)) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_alize_alise(const char *word, const char *suffix, char *out, size_t out_size)
{
    if (!ends_with(word, "al") || strlen(word) < 3 || !suffix_is_ize_form(suffix)) {
        return false;
    }
    return copy_prefix_suffix(word, strlen(word), suffix, out, out_size);
}

static bool suffix_is_arization_form(const char *suffix)
{
    return (starts_with(suffix, "iz") || starts_with(suffix, "is"))
        && (ends_with(suffix, "e")
            || ends_with(suffix, "es")
            || ends_with(suffix, "ing")
            || ends_with(suffix, "ed")
            || ends_with(suffix, "er")
            || ends_with(suffix, "ers")
            || ends_with(suffix, "ation")
            || ends_with(suffix, "ations")
            || ends_with(suffix, "m")
            || ends_with(suffix, "ms"));
}

static bool rule_arization(const char *word, const char *suffix, char *out, size_t out_size)
{
    if (!ends_with(word, "ar") || strlen(word) < 3 || !suffix_is_arization_form(suffix)) {
        return false;
    }
    return copy_prefix_suffix(word, strlen(word), suffix, out, out_size);
}

static bool rule_ize_variations(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (word_len < 2 || !char_in(word[word_len - 1], "lmnty") || !suffix_is_ize_form(suffix)) {
        return false;
    }
    return copy_prefix_suffix(word, word_len, suffix, out, out_size);
}

static bool rule_al_ology(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "al") || word_len < 3 || !starts_with(suffix, "olog")) {
        return false;
    }
    if (!ends_with(suffix, "y")
        && !ends_with(suffix, "ist")
        && !ends_with(suffix, "ists")
        && !ends_with(suffix, "ical")
        && !ends_with(suffix, "ically")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 2, suffix, out, out_size);
}

static bool rule_ish(const char *word, const char *suffix, char *out, size_t out_size)
{
    if ((!ends_with(word, "ar") && !ends_with(word, "er") && !ends_with(word, "or"))
        || strlen(word) < 3
        || !ends_with(suffix, "ish")) {
        return false;
    }
    return copy_prefix_suffix(word, strlen(word), suffix, out, out_size);
}

static bool rule_eed(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "ee") || word_len < 3 || !starts_with(suffix, "e")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_silent_e_ing(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    if (!ends_with(word, "e") || word_len < 3 || suffix[0] == '\0') {
        return false;
    }
    const char before_e = word[word_len - 2];
    if ((!is_consonant(before_e) && before_e != 'u') || !char_in(suffix[0], "aeiouy")) {
        return false;
    }
    return copy_prefix_suffix(word, word_len - 1, suffix, out, out_size);
}

static bool rule_double_consonant_suffix(const char *word, const char *suffix, char *out, size_t out_size)
{
    const size_t word_len = strlen(word);
    const size_t suffix_len = strlen(suffix);
    if (word_len < 3 || suffix_len == 0) {
        return false;
    }

    const char final = word[word_len - 1];
    if (!char_in(final, "bcdfgklmnprtvz") || !char_in(word[word_len - 2], "aeiou")) {
        return false;
    }

    const char antepenult = word[word_len - 3];
    if (!char_in(antepenult, "bcdfghjklmnprstvwxyz")) {
        if (word_len < 4 || memcmp(word + word_len - 4, "qu", 2) != 0) {
            return false;
        }
    }
    if (!char_in(suffix[0], "aeiouy")) {
        return false;
    }

    char doubled[2] = { final, '\0' };
    return write_parts(out, out_size, word, word_len, doubled, 1, suffix, suffix_len);
}

static const Orthography_Rule ORTHOGRAPHY_RULES[] = {
    { "icAlly", rule_ic_ally },
    { "leLy", rule_le_ly },
    { "teOry", rule_te_ory },
    { "mAtory", rule_m_atory },
    { "seAry", rule_se_ary },
    { "tCy", rule_t_cy },
    { "sibilantPlural", rule_sibilant_plural },
    { "chPlural", rule_ch_plural },
    { "consonantYPlural", rule_consonant_y_plural },
    { "ying", rule_ying },
    { "ieEd", rule_ie_ed },
    { "yist", rule_yist },
    { "yToI", rule_y_to_i },
    { "tten", rule_tten },
    { "aeEn", rule_ae_en },
    { "yial", rule_yial },
    { "ification", rule_ification },
    { "yify", rule_yify },
    { "ical", rule_ical },
    { "ological", rule_ological },
    { "yical", rule_yical },
    { "alist", rule_alist },
    { "arity", rule_arity },
    { "ality", rule_ality },
    { "mative", rule_mative },
    { "eative", rule_eative },
    { "ize_ise", rule_ize_ise },
    { "alize_alise", rule_alize_alise },
    { "arization", rule_arization },
    { "ize_variations", rule_ize_variations },
    { "al_ology", rule_al_ology },
    { "ish", rule_ish },
    { "eed", rule_eed },
    { "silent_e_ing", rule_silent_e_ing },
    { "doubleConsonantSuffix", rule_double_consonant_suffix },
};

static bool orthography_contains_word(const Orthography *orthography, const char *word)
{
    if (orthography == NULL || orthography->words == NULL || word == NULL) {
        return false;
    }
    Orthography_Word *words = orthography->words;
    return shgeti(words, word) >= 0;
}

static bool apply_rules_with_dictionary(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    char *out,
    size_t out_size
)
{
    for (size_t i = 0; i < sizeof(ORTHOGRAPHY_RULES) / sizeof(ORTHOGRAPHY_RULES[0]); ++i) {
        (void)ORTHOGRAPHY_RULES[i].name;
        if (ORTHOGRAPHY_RULES[i].apply(word, suffix, out, out_size)
            && orthography_contains_word(orthography, out)) {
            return true;
        }
    }
    return false;
}

static const char *alias_for_suffix(const char *suffix)
{
    if (string_equals(suffix, "able")) {
        return "ible";
    }
    if (string_equals(suffix, "ability")) {
        return "ibility";
    }
    return NULL;
}

static bool copy_with_rest(const char *candidate, const char *rest, char **out_result)
{
    const size_t candidate_len = strlen(candidate);
    const size_t rest_len = strlen(rest);
    char *result = malloc(candidate_len + rest_len + 1);
    if (result == NULL) {
        return false;
    }

    memcpy(result, candidate, candidate_len);
    memcpy(result + candidate_len, rest, rest_len);
    result[candidate_len + rest_len] = '\0';
    *out_result = result;
    return true;
}

bool orthography_load(Orthography *orthography, const char *path)
{
    if (orthography == NULL || path == NULL) {
        return false;
    }

    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        fprintf(stderr, "stoin: failed to read orthography word list '%s'\n", path);
        return false;
    }

    if (orthography->words == NULL) {
        sh_new_strdup(orthography->words);
    }

    char *cursor = file;
    while (*cursor != '\0') {
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        char *word_start = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        if (cursor > word_start) {
            const char saved = *cursor;
            *cursor = '\0';
            shput(orthography->words, word_start, 1);
            *cursor = saved;
        }

        while (*cursor != '\0' && *cursor != '\n') {
            ++cursor;
        }
    }

    free(file);
    return true;
}

void orthography_destroy(Orthography *orthography)
{
    if (orthography == NULL) {
        return;
    }
    shfree(orthography->words);
    orthography->words = NULL;
}

size_t orthography_word_count(const Orthography *orthography)
{
    return orthography == NULL ? 0 : shlenu(orthography->words);
}

bool orthography_apply(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    char **out_result
)
{
    if (word == NULL || suffix == NULL || out_result == NULL) {
        return false;
    }
    *out_result = NULL;

    const char *rest = strchr(suffix, ' ');
    char *core_suffix = NULL;
    if (rest == NULL) {
        core_suffix = copy_cstring(suffix);
        rest = "";
    } else {
        core_suffix = copy_range(suffix, (size_t)(rest - suffix));
    }
    if (core_suffix == NULL) {
        return false;
    }

    const size_t out_size = strlen(word) + strlen(core_suffix) + 32;
    char *candidate = malloc(out_size);
    char *first_non_dictionary_match = NULL;
    if (candidate == NULL) {
        free(core_suffix);
        return false;
    }

    const char *alias = alias_for_suffix(core_suffix);
    if (alias != NULL && apply_rules_with_dictionary(orthography, word, alias, candidate, out_size)) {
        const bool ok = copy_with_rest(candidate, rest, out_result);
        free(candidate);
        free(core_suffix);
        return ok;
    }

    for (size_t i = 0; i < sizeof(ORTHOGRAPHY_RULES) / sizeof(ORTHOGRAPHY_RULES[0]); ++i) {
        (void)ORTHOGRAPHY_RULES[i].name;
        if (ORTHOGRAPHY_RULES[i].apply(word, core_suffix, candidate, out_size)) {
            if (orthography_contains_word(orthography, candidate)) {
                const bool ok = copy_with_rest(candidate, rest, out_result);
                free(first_non_dictionary_match);
                free(candidate);
                free(core_suffix);
                return ok;
            }
            if (first_non_dictionary_match == NULL) {
                first_non_dictionary_match = copy_cstring(candidate);
                if (first_non_dictionary_match == NULL) {
                    free(candidate);
                    free(core_suffix);
                    return false;
                }
            }
        }
    }

    if (!copy_prefix_suffix(word, strlen(word), core_suffix, candidate, out_size)) {
        free(first_non_dictionary_match);
        free(candidate);
        free(core_suffix);
        return false;
    }

    if (orthography_contains_word(orthography, candidate)) {
        const bool ok = copy_with_rest(candidate, rest, out_result);
        free(first_non_dictionary_match);
        free(candidate);
        free(core_suffix);
        return ok;
    }

    const bool ok = copy_with_rest(
        first_non_dictionary_match != NULL ? first_non_dictionary_match : candidate,
        rest,
        out_result
    );
    free(first_non_dictionary_match);
    free(candidate);
    free(core_suffix);
    return ok;
}
