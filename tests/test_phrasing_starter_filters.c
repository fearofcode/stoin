#include "test_support.h"

#include "phrasing.h"
#include "steno_stroke.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool write_phrasing_fixture(const char *path, const char *starters_json)
{
    char json[8192] = {0};
    const int length = snprintf(json, sizeof(json),
        "{\n"
        "  \"initial_verbs\": {\"tails\": [], \"stems\": []},\n"
        "  \"final_verbs\": {\n"
        "    \"contraction_stroke\": \"U\",\n"
        "    \"starters\": [%s],\n"
        "    \"operators\": [\n"
        "      {\"stroke\":\"\",\"modal\":\"none\",\"negative\":false},\n"
        "      {\"stroke\":\"AO\",\"modal\":\"will\",\"negative\":false}\n"
        "    ],\n"
        "    \"structures\": [{\"stroke\":\"\",\"kind\":\"simple\"}],\n"
        "    \"verbs\": [\n"
        "      {\"id\":\"be\",\"base\":\"be\",\"third\":\"is\",\"past\":\"was\","
            "\"present_participle\":\"being\",\"past_participle\":\"been\"},\n"
        "      {\"id\":\"have\",\"base\":\"have\",\"third\":\"has\",\"past\":\"had\","
            "\"present_participle\":\"having\",\"past_participle\":\"had\"}\n"
        "    ],\n"
        "    \"enders\": [\n"
        "      {\"stroke\":\"\",\"past\":false},\n"
        "      {\"stroke\":\"-B\",\"verb\":\"be\",\"past\":false},\n"
        "      {\"stroke\":\"-T\",\"verb\":\"have\",\"past\":false}\n"
        "    ]\n"
        "  }\n"
        "}\n",
        starters_json);
    if (length < 0 || (size_t)length >= sizeof(json)) {
        fputs("test failed: phrasing starter-filter fixture exceeded buffer\n", stderr);
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
    const Phrase_Lookup_Result result = phrasing_lookup(phrasing, bits, &text);
    bool ok = true;
    if (expected == NULL) {
        ok = expect_size(name, result, PHRASE_LOOKUP_MISS)
            && expect_size("starter-filter miss has no text", text != NULL ? 1 : 0, 0);
    } else {
        ok = expect_size(name, result, PHRASE_LOOKUP_HIT)
            && expect_string(name, text, expected);
    }
    free(text);
    return ok;
}

static bool expect_invalid_starter_allowlist(
    const char *path,
    const char *name,
    const char *enders_field
)
{
    char starter[1024] = {0};
    const int length = snprintf(starter, sizeof(starter),
        "{\"stroke\":\"SWHR\",\"text\":\"I\",\"agreement\":\"first_singular\",%s}",
        enders_field);
    if (length < 0 || (size_t)length >= sizeof(starter)) {
        fputs("test failed: invalid phrasing starter-filter fixture exceeded buffer\n", stderr);
        return false;
    }
    if (!write_phrasing_fixture(path, starter)) {
        return false;
    }

    Phrasing *phrasing = phrasing_load(path);
    const bool ok = expect_size(name, phrasing != NULL ? 1 : 0, 0);
    phrasing_destroy(phrasing);
    return ok;
}

bool test_phrasing_starter_filters(void)
{
    const char *path = "build/test-phrasing-starter-filters.json";
    const char *starters =
        "{\"stroke\":\"SWHR\",\"text\":\"I\",\"agreement\":\"first_singular\","
            "\"will_contraction\":\"I'll\",\"enders\":[\"\",\"B\"]},"
        "{\"stroke\":\"KPWH\",\"text\":\"it\",\"agreement\":\"third_singular\"},"
        "{\"stroke\":\"STWR\",\"text\":\"we\",\"agreement\":\"plural\",\"enders\":[]}";

    bool ok = write_phrasing_fixture(path, starters);
    Phrasing *phrasing = ok ? phrasing_load(path) : NULL;
    ok = expect_size("valid FV starter ender allowlists load", phrasing != NULL ? 1 : 0, 1) && ok;
    if (phrasing != NULL) {
        ok = expect_phrase_lookup(phrasing, "FV allowlist permits listed lexical ender", "SWHR-B", "I am") && ok;
        ok = expect_phrase_lookup(phrasing, "FV allowlist permits empty auxiliary ender", "SWHRAO", "I will") && ok;
        ok = expect_phrase_lookup(phrasing, "FV allowlist rejects omitted ender", "SWHR-T", NULL) && ok;
        ok = expect_phrase_lookup(phrasing, "absent FV allowlist permits every ender", "KPWH-T", "it has") && ok;
        ok = expect_phrase_lookup(phrasing, "absent FV allowlist remains starter-local", "KPWH-B", "it is") && ok;
        ok = expect_phrase_lookup(phrasing, "empty FV allowlist permits no enders", "STWR-B", NULL) && ok;
    }
    phrasing_destroy(phrasing);

    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter ender allowlist must be an array",
        "\"enders\":{}") && ok;
    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter ender allowlist values must be strings",
        "\"enders\":[1]") && ok;
    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter ender allowlist rejects invalid outlines",
        "\"enders\":[\"/\"]") && ok;
    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter ender allowlist rejects unknown enders",
        "\"enders\":[\"-Z\"]") && ok;
    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter ender allowlist rejects duplicate ender aliases",
        "\"enders\":[\"-B\",\"B\"]") && ok;
    ok = expect_invalid_starter_allowlist(
        path,
        "FV starter d contraction must be a string",
        "\"d_contraction\":42") && ok;

    remove(path);
    return ok;
}
