#include "test_support.h"

#include "phrasing.h"
#include "steno_stroke.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool write_phrasing_fixture(
    const char *path,
    const char *tails_json,
    const char *stems_json
)
{
    char json[8192] = {0};
    const int length = snprintf(json, sizeof(json),
        "{\n"
        "  \"initial_verbs\": {\n"
        "    \"tails\": [%s],\n"
        "    \"stems\": [%s]\n"
        "  },\n"
        "  \"final_verbs\": {\n"
        "    \"contraction_stroke\": \"U\",\n"
        "    \"starters\": [],\n"
        "    \"operators\": [],\n"
        "    \"structures\": [],\n"
        "    \"verbs\": [],\n"
        "    \"enders\": []\n"
        "  }\n"
        "}\n",
        tails_json,
        stems_json);
    if (length < 0 || (size_t)length >= sizeof(json)) {
        fputs("test failed: phrasing tail-filter fixture exceeded buffer\n", stderr);
        return false;
    }
    return write_text_file(path, json);
}

static bool expect_phrase_lookup(
    const Phrasing *phrasing,
    const char *name,
    const char *stroke,
    const char *expected
)
{
    uint64_t bits = 0;
    if (!stroke_string_to_bits(stroke, &bits)) {
        fprintf(stderr, "test failed: %s could not parse stroke '%s'\n", name, stroke);
        return false;
    }

    char *text = NULL;
    const Phrase_Lookup_Result result = phrasing_lookup(
        phrasing,
        PHRASE_NAMESPACE_INITIAL_VERB,
        bits,
        &text
    );
    bool ok = true;
    if (expected == NULL) {
        ok = expect_size(name, result, PHRASE_LOOKUP_MISS)
            && expect_size("tail-filter miss has no text", text != NULL ? 1 : 0, 0);
    } else {
        ok = expect_size(name, result, PHRASE_LOOKUP_HIT)
            && expect_string(name, text, expected);
    }
    free(text);
    return ok;
}

static bool expect_invalid_tail_allowlist(
    const char *path,
    const char *name,
    const char *tails_json,
    const char *stem_tails_field
)
{
    char stem[1024] = {0};
    const int length = snprintf(stem, sizeof(stem),
        "{\"stroke\":\"PW\",%s\"forms\":[{\"stroke\":\"\",\"text\":\"is\"}]}",
        stem_tails_field);
    if (length < 0 || (size_t)length >= sizeof(stem)) {
        fputs("test failed: invalid phrasing tail-filter fixture exceeded buffer\n", stderr);
        return false;
    }
    if (!write_phrasing_fixture(path, tails_json, stem)) {
        return false;
    }

    Phrasing *phrasing = phrasing_load(path);
    const bool ok = expect_size(name, phrasing != NULL ? 1 : 0, 0);
    phrasing_destroy(phrasing);
    return ok;
}

bool test_phrasing_tail_filters(void)
{
    const char *path = "build/test-phrasing-tail-filters.json";
    const char *tails =
        "{\"id\":\"a\",\"stroke\":\"-B\",\"text\":\"a\"},"
        "{\"id\":\"an\",\"stroke\":\"-PB\",\"text\":\"an\"},"
        "{\"id\":\"like\",\"stroke\":\"-BL\",\"text\":\"like\"},"
        "{\"id\":\"the\",\"stroke\":\"-T\",\"text\":\"the\"},"
        "{\"id\":\"us\",\"stroke\":\"-S\",\"text\":\"us\"},"
        "{\"id\":\"he\",\"stroke\":\"-RPB\",\"text\":\"he\","
            "\"stems\":[\"PW\"],\"forms\":[\"\",\"-D\",\"E-D\"]},"
        "{\"id\":\"she\",\"stroke\":\"-RB\",\"text\":\"she\","
            "\"stems\":[\"PW\"],\"forms\":[\"\",\"-D\",\"E-D\"]}";
    const char *stems =
        "{\"stroke\":\"PW\",\"tails\":[\"a\",\"an\",\"like\",\"us\",\"he\",\"she\"],"
            "\"forms\":[{\"stroke\":\"\",\"text\":\"is\"},"
                "{\"stroke\":\"-D\",\"text\":\"was\"},"
                "{\"stroke\":\"E\",\"text\":\"are\"},"
                "{\"stroke\":\"E-D\",\"text\":\"were\"}]},"
        "{\"stroke\":\"H\","
            "\"forms\":[{\"stroke\":\"\",\"text\":\"has\"}]},"
        "{\"stroke\":\"ST\",\"tails\":[],"
            "\"forms\":[{\"stroke\":\"\",\"text\":\"says\"}]}";

    bool ok = write_phrasing_fixture(path, tails, stems);
    Phrasing *phrasing = ok ? phrasing_load(path) : NULL;
    ok = expect_size("valid IV tail allowlists load", phrasing != NULL ? 1 : 0, 1) && ok;
    if (phrasing != NULL) {
        ok = expect_phrase_lookup(phrasing, "IV allowlist permits first tail", "PW-B", "is a") && ok;
        ok = expect_phrase_lookup(phrasing, "IV allowlist permits multi-key tail", "PW-PB", "is an") && ok;
        ok = expect_phrase_lookup(phrasing, "IV allowlist permits like tail", "PW-BL", "is like") && ok;
        ok = expect_phrase_lookup(phrasing, "IV allowlist permits later tail", "PW-S", "is us") && ok;
        ok = expect_phrase_lookup(phrasing, "IV allowlist rejects omitted tail", "PW-T", NULL) && ok;
        ok = expect_phrase_lookup(phrasing, "absent IV allowlist permits every tail", "H-T", "has the") && ok;
        ok = expect_phrase_lookup(phrasing, "empty IV allowlist permits no tails", "ST-B", NULL) && ok;
        ok = expect_phrase_lookup(phrasing, "tail stem and form filters permit he", "PW-RPB", "is he") && ok;
        ok = expect_phrase_lookup(phrasing, "tail stem and form filters permit she", "PW-RB", "is she") && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter permits was he", "PW-RPBD", "was he") && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter permits was she", "PW-RBD", "was she") && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter permits were he", "PWE-RPBD", "were he") && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter permits were she", "PWE-RBD", "were she") && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter rejects are he", "PWE-RPB", NULL) && ok;
        ok = expect_phrase_lookup(phrasing, "tail form filter rejects are she", "PWE-RB", NULL) && ok;
        ok = expect_phrase_lookup(phrasing, "tail stem filter rejects has he", "H-RPB", NULL) && ok;
    }
    phrasing_destroy(phrasing);

    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail allowlist must be an array",
        tails,
        "\"tails\":{},") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail allowlist values must be strings",
        tails,
        "\"tails\":[1],") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail allowlist rejects unknown IDs",
        tails,
        "\"tails\":[\"missing\"],") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail allowlist rejects duplicate IDs",
        tails,
        "\"tails\":[\"a\",\"a\"],") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "initial verb tails reject duplicate IDs",
        "{\"id\":\"a\",\"stroke\":\"-B\",\"text\":\"a\"},"
        "{\"id\":\"a\",\"stroke\":\"-T\",\"text\":\"another\"}",
        "") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail form allowlist must be an array",
        "{\"id\":\"a\",\"stroke\":\"-B\",\"text\":\"a\",\"forms\":{}}",
        "") && ok;
    ok = expect_invalid_tail_allowlist(
        path,
        "IV tail form allowlist rejects duplicate outlines",
        "{\"id\":\"a\",\"stroke\":\"-B\",\"text\":\"a\",\"forms\":[\"E\",\"E\"]}",
        "") && ok;

    remove(path);
    return ok;
}
