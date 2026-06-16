#include "gemini_pr.h"
#include "orthography.h"
#include "platform.h"
#include "steno.h"
#include "steno_stroke.h"
#include "tx_bolt.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Test_Output {
    char *text;
    char last_send[128];
    char last_delete[128];
    char last_key_combo[128];
    size_t send_count;
    size_t delete_count;
    size_t key_combo_count;
} Test_Output;

typedef struct Watch_Test {
    Steno *steno;
    size_t reload_count;
} Watch_Test;

enum {
    TEST_KEYCODE_ESCAPE = 53,
    TEST_KEYCODE_LEFT_CONTROL = 59,
};

static bool test_send_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
    ++output->send_count;
    snprintf(output->last_send, sizeof(output->last_send), "%s", utf8);

    if (output->text != NULL && arrlenu(output->text) > 0) {
        arrpop(output->text);
    }
    for (const char *p = utf8; *p != '\0'; ++p) {
        arrput(output->text, *p);
    }
    arrput(output->text, '\0');
    return true;
}

static bool test_delete_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
    const size_t delete_length = strlen(utf8);
    if (delete_length == 0) {
        return true;
    }
    if (output->text == NULL) {
        return false;
    }

    const size_t length = strlen(output->text);
    if (delete_length > length) {
        return false;
    }
    if (memcmp(output->text + length - delete_length, utf8, delete_length) != 0) {
        return false;
    }

    ++output->delete_count;
    snprintf(output->last_delete, sizeof(output->last_delete), "%s", utf8);

    const size_t new_length = length - delete_length;
    arrsetlen(output->text, new_length + 1);
    output->text[new_length] = '\0';
    return true;
}

static bool test_send_key_combination(const char *combo, void *userdata)
{
    Test_Output *output = userdata;
    ++output->key_combo_count;
    snprintf(output->last_key_combo, sizeof(output->last_key_combo), "%s", combo);
    return true;
}

static void clear_test_output(Test_Output *output)
{
    arrsetlen(output->text, 0);
    arrput(output->text, '\0');
}

static void reset_output_log(Test_Output *output)
{
    output->last_send[0] = '\0';
    output->last_delete[0] = '\0';
    output->last_key_combo[0] = '\0';
    output->send_count = 0;
    output->delete_count = 0;
    output->key_combo_count = 0;
}

static bool write_text_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    if (fputs(contents, file) == EOF) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static void test_dictionary_watch_callback(void *userdata)
{
    Watch_Test *watch = userdata;
    if (watch == NULL) {
        return;
    }
    if (steno_reload_dictionary(watch->steno)) {
        ++watch->reload_count;
    }
}

static bool send_key_event(Steno *steno, const char *key_name, bool is_down)
{
    uint16_t keycode = 0;
    if (!platform_keycode_from_name(key_name, &keycode)) {
        return false;
    }

    Input_Event event = {
        .keycode = keycode,
        .is_down = is_down,
    };
    return steno_handle_event(steno, &event);
}

static bool send_raw_key_event(
    Steno *steno,
    uint16_t keycode,
    bool is_down,
    bool control,
    bool option,
    bool command
)
{
    const Input_Event event = {
        .keycode = keycode,
        .is_down = is_down,
        .control = control,
        .option = option,
        .command = command,
    };
    return steno_handle_event(steno, &event);
}

static bool expect_string(const char *name, const char *actual, const char *expected)
{
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected '%s', got '%s'\n",
        name,
        expected,
        actual == NULL ? "(null)" : actual);
    return false;
}

static bool expect_size_at_most(const char *name, size_t actual, size_t expected_max)
{
    if (actual <= expected_max) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected at most %zu, got %zu\n",
        name,
        expected_max,
        actual);
    return false;
}

static bool expect_size(const char *name, size_t actual, size_t expected)
{
    if (actual == expected) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected %zu, got %zu\n",
        name,
        expected,
        actual);
    return false;
}

static bool expect_stroke_format(const char *input, const char *expected)
{
    uint64_t bits = 0;
    char output[64] = {0};

    if (!stroke_string_to_bits(input, &bits)) {
        fprintf(stderr, "test failed: could not parse stroke '%s'\n", input);
        return false;
    }
    if (!chord_bits_to_string(bits, output, sizeof(output))) {
        fprintf(stderr, "test failed: could not format stroke '%s'\n", input);
        return false;
    }

    return expect_string(input, output, expected);
}

static bool expect_orthography(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    const char *expected
)
{
    char *actual = NULL;
    if (!orthography_apply(orthography, word, suffix, &actual)) {
        fprintf(stderr, "test failed: orthography %s + %s failed\n", word, suffix);
        return false;
    }

    const bool ok = expect_string("orthography", actual, expected);
    free(actual);
    return ok;
}

static bool read_trace_line(FILE *trace_file, char *line, size_t line_size)
{
    fflush(trace_file);
    rewind(trace_file);
    if (fgets(line, (int)line_size, trace_file) == NULL) {
        return false;
    }
    return true;
}

int main(void)
{
    Test_Output output = {0};
    Steno_Config config = {
        .keymap_path = "tests/test.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .word_list_path = "tests/test-words.txt",
        .send_text = test_send_text,
        .delete_text = test_delete_text,
        .send_key_combination = test_send_key_combination,
        .send_userdata = &output,
    };

    Steno *steno = steno_create(&config);
    if (steno == NULL) {
        fputs("test failed: could not create steno engine\n", stderr);
        return 1;
    }

    bool ok = true;

    Orthography test_orthography = {0};
    ok = ok && orthography_load(&test_orthography, "tests/test-words.txt");
    ok = ok && orthography_word_count(&test_orthography) > 0;
    ok = ok && expect_orthography(&test_orthography, "artistic", "ly", "artistically");
    ok = ok && expect_orthography(&test_orthography, "cosmetic", "ly", "cosmetically");
    ok = ok && expect_orthography(&test_orthography, "establish", "s", "establishes");
    ok = ok && expect_orthography(&test_orthography, "speech", "s", "speeches");
    ok = ok && expect_orthography(&test_orthography, "approach", "s", "approaches");
    ok = ok && expect_orthography(&test_orthography, "beach", "s", "beaches");
    ok = ok && expect_orthography(&test_orthography, "arch", "s", "arches");
    ok = ok && expect_orthography(&test_orthography, "larch", "s", "larches");
    ok = ok && expect_orthography(&test_orthography, "march", "s", "marches");
    ok = ok && expect_orthography(&test_orthography, "search", "s", "searches");
    ok = ok && expect_orthography(&test_orthography, "starch", "s", "starches");
    ok = ok && expect_orthography(&test_orthography, "stomach", "s", "stomachs");
    ok = ok && expect_orthography(&test_orthography, "monarch", "s", "monarchs");
    ok = ok && expect_orthography(&test_orthography, "patriarch", "s", "patriarchs");
    ok = ok && expect_orthography(&test_orthography, "oligarch", "s", "oligarchs");
    ok = ok && expect_orthography(&test_orthography, "cherry", "s", "cherries");
    ok = ok && expect_orthography(&test_orthography, "day", "s", "days");
    ok = ok && expect_orthography(&test_orthography, "penny", "s", "pennies");
    ok = ok && expect_orthography(&test_orthography, "pharmacy", "ist", "pharmacist");
    ok = ok && expect_orthography(&test_orthography, "melody", "ist", "melodist");
    ok = ok && expect_orthography(&test_orthography, "pacify", "ist", "pacifist");
    ok = ok && expect_orthography(&test_orthography, "geology", "ist", "geologist");
    ok = ok && expect_orthography(&test_orthography, "metallurgy", "ist", "metallurgist");
    ok = ok && expect_orthography(&test_orthography, "anarchy", "ist", "anarchist");
    ok = ok && expect_orthography(&test_orthography, "monopoly", "ist", "monopolist");
    ok = ok && expect_orthography(&test_orthography, "alchemy", "ist", "alchemist");
    ok = ok && expect_orthography(&test_orthography, "similar", "ish", "similarish");
    ok = ok && expect_orthography(&test_orthography, "red", "ish", "reddish");
    ok = ok && expect_orthography(&test_orthography, "tinker", "er", "tinkerer");
    ok = ok && expect_orthography(&test_orthography, "filter", "er", "filterer");
    orthography_destroy(&test_orthography);

    uint64_t rr_bits = 0;
    char rr_string[64] = {0};
    ok = ok && stroke_string_to_bits("R-R", &rr_bits);
    ok = ok && chord_bits_to_string(rr_bits, rr_string, sizeof(rr_string));
    ok = ok && expect_string("R-R canonicalization", rr_string, "R-R");
    ok = ok && expect_stroke_format("SA-P", "SAP");
    ok = ok && expect_stroke_format("TAT", "TAT");
    ok = ok && expect_stroke_format("R-R", "R-R");
    ok = ok && expect_stroke_format("-T", "-T");
    ok = ok && expect_stroke_format("-F", "F");
    const char *drill_chords[] = {
        "SAP", "HUD", "SOG", "TOD", "WET", "POG", "ROD", "KUS", "PEB", "ROR",
        "WEZ", "WEL", "TER", "TAT", "WEF", "KAB", "WES", "SAP", "TAS", "RET",
        "TAD", "PEP", "SEB", "KOF", "TUZ", "PEF", "HEL", "PUB", "RAT", "WAF",
        "TAB", "RAS", "HUP", "WUP", "PEZ", "SOF", "HUR", "PUZ", "SOB", "POT",
        "KED", "WUD", "SAG", "RAP", "RAL", "ROL", "WOZ", "KAD", "KAT", "KOB",
        "RAD", "TAR", "SAL", "ROF", "SOR", "WOT", "HUF", "TUR", "KAF", "HOR",
        "SOD", "KOT", "SEF", "RED", "HAP", "PAP", "KEG", "KOZ", "TUS", "SOZ",
        "TAG", "HAS", "TAF", "HES", "HOL", "WUR", "TEB", "HAB", "HER", "PER",
        "TOP", "HAZ", "POL", "WOS", "HOP", "SUT", "TOR", "REL", "PAT", "SER",
        "WUS", "PUP", "KAG", "POD", "SUB", "HED", "SAB", "SUL", "TEF", "SOL",
    };
    for (size_t i = 0; i < sizeof(drill_chords) / sizeof(drill_chords[0]); ++i) {
        ok = ok && expect_stroke_format(drill_chords[i], drill_chords[i]);
    }

    uint64_t gemini_bits = 0;
    const uint8_t gemini_sat[GEMINI_PR_PACKET_SIZE] = { 0x80, 0x40, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && gemini_pr_decode_packet(gemini_sat, &gemini_bits);
    char gemini_sat_string[64] = {0};
    ok = ok && chord_bits_to_string(gemini_bits, gemini_sat_string, sizeof(gemini_sat_string));
    ok = ok && expect_string("Gemini PR SAT packet", gemini_sat_string, "SAT");

    const uint8_t gemini_number_star_z[GEMINI_PR_PACKET_SIZE] = { 0xA0, 0x00, 0x08, 0x00, 0x00, 0x01 };
    ok = ok && gemini_pr_decode_packet(gemini_number_star_z, &gemini_bits);
    char gemini_number_star_z_string[64] = {0};
    ok = ok && chord_bits_to_string(gemini_bits, gemini_number_star_z_string, sizeof(gemini_number_star_z_string));
    ok = ok && expect_string("Gemini PR number star Z packet", gemini_number_star_z_string, "#*Z");

    const uint8_t bad_gemini_start[GEMINI_PR_PACKET_SIZE] = { 0x00, 0x40, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && !gemini_pr_decode_packet(bad_gemini_start, &gemini_bits);
    const uint8_t bad_gemini_continuation[GEMINI_PR_PACKET_SIZE] = { 0x80, 0xC0, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && !gemini_pr_decode_packet(bad_gemini_continuation, &gemini_bits);

    Tx_Bolt tx_bolt = {0};
    uint64_t tx_bolt_bits = 0;
    char tx_bolt_string[64] = {0};
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x01, &tx_bolt_bits);
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x42, &tx_bolt_bits);
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x84, &tx_bolt_bits);
    ok = ok && tx_bolt_flush_stroke(&tx_bolt, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt SAP packet", tx_bolt_string, "SAP");

    memset(&tx_bolt, 0, sizeof(tx_bolt));
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x48, &tx_bolt_bits);
    ok = ok && tx_bolt_decode_byte(&tx_bolt, 0xD8, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt number star Z packet", tx_bolt_string, "#*Z");

    memset(&tx_bolt, 0, sizeof(tx_bolt));
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x01, &tx_bolt_bits);
    ok = ok && tx_bolt_decode_byte(&tx_bolt, 0x02, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt lower set starts new stroke", tx_bolt_string, "S");
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && tx_bolt_flush_stroke(&tx_bolt, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt queued next stroke", tx_bolt_string, "T");

    const char *the = NULL;
    ok = ok && steno_lookup_stroke(steno, "-T", &the);
    ok = ok && expect_string("dictionary lookup -T", the, "the");

    const char *undo = NULL;
    ok = ok && steno_lookup_stroke(steno, "-R", &undo);
    ok = ok && expect_string("dictionary lookup -R", undo, "=undo");

    const char *suffix_s = NULL;
    ok = ok && steno_lookup_stroke(steno, "-S", &suffix_s);
    ok = ok && expect_string("dictionary lookup -S", suffix_s, "{^s}");

    const char *glue_p = NULL;
    ok = ok && steno_lookup_stroke(steno, "P*P", &glue_p);
    ok = ok && expect_string("dictionary lookup P*P", glue_p, "{&P}");

    const char *stories = NULL;
    ok = ok && steno_lookup_stroke(steno, "STOE-R/-Z", &stories);
    ok = ok && expect_string("dictionary lookup canonical multi-stroke", stories, "stories");

    const char *histories = NULL;
    ok = ok && steno_lookup_stroke(steno, "HEU/STOE-R/-Z", &histories);
    ok = ok && expect_string("dictionary lookup longest multi-stroke", histories, "histories");

    const char *reload_path = "build/test-hot-reload-dictionary.json";
    ok = ok && write_text_file(reload_path, "{ \"S\": \"old\" }\n");
    Steno_Config reload_config = config;
    reload_config.dictionary_path = reload_path;
    Steno *reload_steno = steno_create(&reload_config);
    ok = ok && reload_steno != NULL;
    if (reload_steno != NULL) {
        uint64_t reload_bits = 0;
        ok = ok && stroke_string_to_bits("S", &reload_bits);

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload initial dictionary", output.text, "old ");

        ok = ok && write_text_file(reload_path, "{");
        ok = ok && !steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload keeps old dictionary on parse failure", output.text, "old ");

        ok = ok && write_text_file(reload_path, "{ \"S\": \"newer\" }\n");
        ok = ok && steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload updated dictionary", output.text, "newer ");

        Watch_Test watch = {
            .steno = reload_steno,
        };
        const char *const watch_paths[] = { reload_path };
        ok = ok && platform_file_watcher_start(
            watch_paths,
            sizeof(watch_paths) / sizeof(watch_paths[0]),
            test_dictionary_watch_callback,
            &watch
        );
        ok = ok && write_text_file(reload_path, "{ \"S\": \"watched\" }\n");
        for (size_t attempt = 0; ok && watch.reload_count == 0 && attempt < 50; ++attempt) {
            platform_file_watcher_poll();
            platform_sleep_ms(10);
        }
        platform_file_watcher_stop();
        ok = ok && watch.reload_count > 0;
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("platform dictionary watcher reload", output.text, "watched ");

        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
        ok = ok && write_text_file(reload_path, "{ \"S\": \"disabled\" }\n");
        ok = ok && steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && !send_key_event(reload_steno, "a", true);
        ok = ok && !send_key_event(reload_steno, "a", false);
        ok = ok && expect_string("hot reload while capture disabled suppresses output", output.text, "");

        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
        clear_test_output(&output);
        ok = ok && send_key_event(reload_steno, "a", true);
        ok = ok && send_key_event(reload_steno, "a", false);
        ok = ok && expect_string("hot reload while disabled applies after reenable", output.text, "disabled ");

        steno_destroy(reload_steno);
    }
    remove(reload_path);

    const char *dump_path = "build/test-dictionary-dump.json";
    ok = ok && steno_dump_dictionary_json(steno, dump_path);
    size_t dump_size = 0;
    char *dump = read_entire_file(dump_path, &dump_size);
    ok = ok && dump != NULL && dump_size > 0;
    if (dump != NULL) {
        ok = ok && strstr(dump, "\"STOER/Z\": \"stories\"") != NULL;
        free(dump);
    }
    remove(dump_path);

    FILE *trace_file = tmpfile();
    ok = ok && trace_file != NULL;
    if (trace_file != NULL) {
        Steno_Config trace_config = config;
        trace_config.trace_file = trace_file;
        Steno *trace_steno = steno_create(&trace_config);
        ok = ok && trace_steno != NULL;
        if (trace_steno != NULL) {
            char trace_line[128] = {0};
            uint64_t trace_bits = 0;
            clear_test_output(&output);
            ok = ok && stroke_string_to_bits("-T", &trace_bits);
            ok = ok && steno_handle_stroke_bits(trace_steno, trace_bits);
            ok = ok && read_trace_line(trace_file, trace_line, sizeof(trace_line));
            ok = ok && expect_string("trace translated stroke", trace_line, "-T -> the\n");
            steno_destroy(trace_steno);
        }
        fclose(trace_file);
    }

    clear_test_output(&output);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
    ok = ok && !send_key_event(steno, "u", true);
    ok = ok && !send_key_event(steno, "u", false);
    ok = ok && expect_string("ctrl escape disables capture", output.text, "");

    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("ctrl escape reenables capture", output.text, "fee ");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("first undoable translation", output.text, "fee ");
    ok = ok && send_key_event(steno, "i", true);
    ok = ok && send_key_event(steno, "i", false);
    ok = ok && expect_string("second undoable translation", output.text, "fee pay ");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("one level undo", output.text, "fee ");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("two level undo", output.text, "");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("empty undo stack", output.text, "");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "r", true);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "r", false);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("unicode undoable translation", output.text, "caffè ");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("unicode undo", output.text, "");

    uint64_t story_bits = 0;
    uint64_t plural_bits = 0;
    uint64_t past_bits = 0;
    uint64_t history_bits = 0;
    uint64_t undo_bits = 0;
    uint64_t filler_bits = 0;
    uint64_t sap_bits = 0;
    uint64_t saps_bits = 0;
    uint64_t er_bits = 0;
    uint64_t erz_bits = 0;
    uint64_t cat_bits = 0;
    uint64_t stitch_a_bits = 0;
    uint64_t stitch_b_bits = 0;
    uint64_t stitch_c_bits = 0;
    uint64_t test_bits = 0;
    uint64_t eye_bits = 0;
    uint64_t to_bits = 0;
    uint64_t hyphen_bits = 0;
    uint64_t stitch_word_bits = 0;
    uint64_t suffix_s_bits = 0;
    uint64_t red_bits = 0;
    uint64_t cherry_bits = 0;
    uint64_t cherries_bits = 0;
    uint64_t defer_bits = 0;
    uint64_t deferred_bits = 0;
    uint64_t failing_bits = 0;
    uint64_t suffix_ish_bits = 0;
    uint64_t raw_ish_bits = 0;
    uint64_t prefix_bits = 0;
    uint64_t port_bits = 0;
    uint64_t delete_space_bits = 0;
    uint64_t force_space_bits = 0;
    uint64_t one_bits = 0;
    uint64_t two_bits = 0;
    uint64_t glue_p_bits = 0;
    uint64_t basket_bits = 0;
    uint64_t ball_bits = 0;
    uint64_t toggle_star_bits = 0;
    uint64_t retro_delete_space_bits = 0;
    uint64_t retro_insert_space_bits = 0;
    uint64_t caps_mode_bits = 0;
    uint64_t reset_mode_bits = 0;
    uint64_t snake_mode_bits = 0;
    uint64_t camel_mode_bits = 0;
    uint64_t lower_mode_bits = 0;
    uint64_t title_mode_bits = 0;
    uint64_t empty_space_mode_bits = 0;
    uint64_t reset_space_mode_bits = 0;
    uint64_t repeat_bits = 0;
    uint64_t period_bits = 0;
    uint64_t comma_bits = 0;
    uint64_t cap_next_bits = 0;
    uint64_t upper_next_bits = 0;
    uint64_t lower_next_bits = 0;
    uint64_t lower_previous_bits = 0;
    uint64_t plover_bits = 0;
    uint64_t right_arrow_bits = 0;
    uint64_t modal_toggle_bits = 0;
    ok = ok && stroke_string_to_bits("STOER", &story_bits);
    ok = ok && stroke_string_to_bits("-Z", &plural_bits);
    ok = ok && stroke_string_to_bits("-D", &past_bits);
    ok = ok && stroke_string_to_bits("HEU", &history_bits);
    ok = ok && stroke_string_to_bits("-R", &undo_bits);
    ok = ok && stroke_string_to_bits("#", &filler_bits);
    ok = ok && stroke_string_to_bits("SAP", &sap_bits);
    ok = ok && stroke_string_to_bits("SAPS", &saps_bits);
    ok = ok && stroke_string_to_bits("*ER", &er_bits);
    ok = ok && stroke_string_to_bits("*ERZ", &erz_bits);
    ok = ok && stroke_string_to_bits("KAT", &cat_bits);
    ok = ok && stroke_string_to_bits("A", &stitch_a_bits);
    ok = ok && stroke_string_to_bits("PW", &stitch_b_bits);
    ok = ok && stroke_string_to_bits("KR", &stitch_c_bits);
    ok = ok && stroke_string_to_bits("TEFT", &test_bits);
    ok = ok && stroke_string_to_bits("AOEU", &eye_bits);
    ok = ok && stroke_string_to_bits("TO", &to_bits);
    ok = ok && stroke_string_to_bits("H-PB", &hyphen_bits);
    ok = ok && stroke_string_to_bits("-RBGS", &stitch_word_bits);
    ok = ok && stroke_string_to_bits("-S", &suffix_s_bits);
    ok = ok && stroke_string_to_bits("RED", &red_bits);
    ok = ok && stroke_string_to_bits("KHER", &cherry_bits);
    ok = ok && stroke_string_to_bits("KHERZ", &cherries_bits);
    ok = ok && stroke_string_to_bits("TKEFR", &defer_bits);
    ok = ok && stroke_string_to_bits("TKEFRD", &deferred_bits);
    ok = ok && stroke_string_to_bits("TPAEULG", &failing_bits);
    ok = ok && stroke_string_to_bits("EURB", &suffix_ish_bits);
    ok = ok && stroke_string_to_bits("R-R", &raw_ish_bits);
    ok = ok && stroke_string_to_bits("PRAOE", &prefix_bits);
    ok = ok && stroke_string_to_bits("PORT", &port_bits);
    ok = ok && stroke_string_to_bits("TK-LS", &delete_space_bits);
    ok = ok && stroke_string_to_bits("S-P", &force_space_bits);
    ok = ok && stroke_string_to_bits("#S", &one_bits);
    ok = ok && stroke_string_to_bits("#T", &two_bits);
    ok = ok && stroke_string_to_bits("P*P", &glue_p_bits);
    ok = ok && stroke_string_to_bits("PWA", &basket_bits);
    ok = ok && stroke_string_to_bits("PWAL", &ball_bits);
    ok = ok && stroke_string_to_bits("#*", &toggle_star_bits);
    ok = ok && stroke_string_to_bits("SP-LS", &retro_delete_space_bits);
    ok = ok && stroke_string_to_bits("S-PD", &retro_insert_space_bits);
    ok = ok && stroke_string_to_bits("KA*PS", &caps_mode_bits);
    ok = ok && stroke_string_to_bits("R*EFT", &reset_mode_bits);
    ok = ok && stroke_string_to_bits("WRA", &snake_mode_bits);
    ok = ok && stroke_string_to_bits("WRO", &camel_mode_bits);
    ok = ok && stroke_string_to_bits("WRE", &lower_mode_bits);
    ok = ok && stroke_string_to_bits("WRU", &title_mode_bits);
    ok = ok && stroke_string_to_bits("TPHA", &empty_space_mode_bits);
    ok = ok && stroke_string_to_bits("KPAO", &reset_space_mode_bits);
    ok = ok && stroke_string_to_bits("SKWR", &repeat_bits);
    ok = ok && stroke_string_to_bits("TP-PL", &period_bits);
    ok = ok && stroke_string_to_bits("KW-BG", &comma_bits);
    ok = ok && stroke_string_to_bits("KPA", &cap_next_bits);
    ok = ok && stroke_string_to_bits("KPA*L", &upper_next_bits);
    ok = ok && stroke_string_to_bits("HRO*ER", &lower_next_bits);
    ok = ok && stroke_string_to_bits("HRO*ERD", &lower_previous_bits);
    ok = ok && stroke_string_to_bits("PHROF", &plover_bits);
    ok = ok && stroke_string_to_bits("STPH-G", &right_arrow_bits);
    ok = ok && stroke_string_to_bits("STPH", &modal_toggle_bits);

    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, story_bits);
    ok = ok && expect_string("story first stroke", output.text, "story ");
    ok = ok && output.send_count == 1 && output.delete_count == 0;
    ok = ok && expect_string("story send text", output.last_send, "story ");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, plural_bits);
    ok = ok && expect_string("stories retroactive replacement", output.text, "stories ");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("stories minimal delete", output.last_delete, "y ");
    ok = ok && expect_string("stories minimal insert", output.last_send, "ies ");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string("undo restores replaced translation", output.text, "story ");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("undo stories delete", output.last_delete, "ies ");
    ok = ok && expect_string("undo stories insert", output.last_send, "y ");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, past_bits);
    ok = ok && expect_string("past tense after undo uses restored stroke history", output.text, "storied ");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("storied minimal delete", output.last_delete, "y ");
    ok = ok && expect_string("storied minimal insert", output.last_send, "ied ");

    Steno *suffix_key_steno = steno_create(&config);
    ok = ok && suffix_key_steno != NULL;
    if (suffix_key_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, saps_bits);
        ok = ok && expect_string("suffix key single stroke", output.text, "saps ");
        ok = ok && expect_string("suffix key single stroke send", output.last_send, "saps ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, history_bits);
        ok = ok && expect_string("suffix key multi-stroke first raw", output.text, "HEU ");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, saps_bits);
        ok = ok && expect_string("suffix key multi-stroke", output.text, "history saps ");
        ok = ok && expect_string("suffix key multi-stroke delete", output.last_delete, "HEU ");
        ok = ok && expect_string("suffix key multi-stroke insert", output.last_send, "history saps ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, cherries_bits);
        ok = ok && expect_string("suffix key z orthography", output.text, "cherries ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, deferred_bits);
        ok = ok && expect_string("suffix key d orthography", output.text, "deferred ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, failing_bits);
        ok = ok && expect_string("suffix key g", output.text, "failing ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, sap_bits);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, er_bits);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, suffix_s_bits);
        ok = ok && expect_string("separate attach suffix strokes", output.text, "sappers ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, sap_bits);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, erz_bits);
        ok = ok && expect_string("suffix key attaches base suffix to previous word", output.text, "sappers ");
        ok = ok && expect_string("suffix key attach delete", output.last_delete, " ");
        ok = ok && expect_string("suffix key attach insert", output.last_send, "pers ");

        steno_destroy(suffix_key_steno);
    }

    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, history_bits);
    ok = ok && steno_handle_stroke_bits(steno, story_bits);
    ok = ok && steno_handle_stroke_bits(steno, plural_bits);
    ok = ok && expect_string("longest multi-stroke replacement", output.text, "histories ");
    ok = ok && expect_string("longest multi-stroke delete", output.last_delete, "HEU story ");
    ok = ok && expect_string("longest multi-stroke insert", output.last_send, "histories ");

    Steno *compact_steno = steno_create(&config);
    ok = ok && compact_steno != NULL;
    if (compact_steno != NULL) {
        clear_test_output(&output);
        for (size_t i = 0; ok && i < 1999; ++i) {
            ok = ok && steno_handle_stroke_bits(compact_steno, filler_bits);
        }

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, story_bits);
        ok = ok && expect_size_at_most(
            "compacted history stroke count",
            steno_translation_history_stroke_count(compact_steno),
            1000
        );
        ok = ok && expect_string("compaction keeps current stroke output", output.text, "story ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, plural_bits);
        ok = ok && expect_string("retro translation after compaction", output.text, "stories ");
        ok = ok && expect_string("retro delete after compaction", output.last_delete, "y ");
        ok = ok && expect_string("retro insert after compaction", output.last_send, "ies ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, undo_bits);
        ok = ok && expect_string("undo after compaction", output.text, "story ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, past_bits);
        ok = ok && expect_string("translation after compacted undo", output.text, "storied ");

        steno_destroy(compact_steno);
    }

    Steno *format_steno = steno_create(&config);
    ok = ok && expect_size("format steno created", format_steno != NULL ? 1 : 0, 1);
    if (format_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("attach base word", output.text, "cat ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_s_bits);
        ok = ok && expect_string("attach suffix", output.text, "cats ");
        ok = ok && expect_size("attach suffix send count", output.send_count, 1);
        ok = ok && expect_size("attach suffix delete count", output.delete_count, 1);
        ok = ok && expect_string("attach suffix delete", output.last_delete, " ");
        ok = ok && expect_string("attach suffix insert", output.last_send, "s ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, undo_bits);
        ok = ok && expect_string("undo attach suffix", output.text, "cat ");
        ok = ok && expect_string("undo attach suffix delete", output.last_delete, "s ");
        ok = ok && expect_string("undo attach suffix insert", output.last_send, " ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, past_bits);
        ok = ok && expect_string("orthographic ed suffix", output.text, "catted ");
        ok = ok && expect_string("orthographic ed delete", output.last_delete, " ");
        ok = ok && expect_string("orthographic ed insert", output.last_send, "ted ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, red_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_ish_bits);
        ok = ok && expect_string("orthographic ish suffix", output.text, "reddish ");
        ok = ok && expect_string("orthographic ish delete", output.last_delete, " ");
        ok = ok && expect_string("orthographic ish insert", output.last_send, "dish ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, red_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, raw_ish_bits);
        ok = ok && expect_string("raw ish suffix", output.text, "redish ");
        ok = ok && expect_string("raw ish delete", output.last_delete, " ");
        ok = ok && expect_string("raw ish insert", output.last_send, "ish ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cherry_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_s_bits);
        ok = ok && expect_string("orthographic y plural", output.text, "cherries ");
        ok = ok && expect_string("orthographic y plural delete", output.last_delete, "y ");
        ok = ok && expect_string("orthographic y plural insert", output.last_send, "ies ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, defer_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, past_bits);
        ok = ok && expect_string("orthographic doubled consonant", output.text, "deferred ");
        ok = ok && expect_string("orthographic doubled consonant delete", output.last_delete, " ");
        ok = ok && expect_string("orthographic doubled consonant insert", output.last_send, "red ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, prefix_bits);
        ok = ok && expect_string("prefix attach first stroke", output.text, "pre");
        ok = ok && expect_string("prefix attach first send", output.last_send, "pre");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, port_bits);
        ok = ok && expect_string("prefix attach next word", output.text, "preport ");
        ok = ok && expect_string("prefix attach next send", output.last_send, "port ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, basket_bits);
        ok = ok && expect_string("delete-space base word", output.text, "basket ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, delete_space_bits);
        ok = ok && expect_string("delete-space command", output.text, "basket");
        ok = ok && expect_size("delete-space send count", output.send_count, 0);
        ok = ok && expect_size("delete-space delete count", output.delete_count, 1);
        ok = ok && expect_string("delete-space delete", output.last_delete, " ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, ball_bits);
        ok = ok && expect_string("delete-space next word", output.text, "basketball ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, prefix_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, force_space_bits);
        ok = ok && expect_string("force-space command", output.text, "pre ");
        ok = ok && expect_string("force-space insert", output.last_send, " ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, port_bits);
        ok = ok && expect_string("force-space next word", output.text, "pre port ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, basket_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, ball_bits);
        ok = ok && expect_string("retro delete-space base phrase", output.text, "basket ball ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, retro_delete_space_bits);
        ok = ok && expect_string("retro delete-space command", output.text, "basketball ");
        ok = ok && expect_string("retro delete-space delete", output.last_delete, " ball ");
        ok = ok && expect_string("retro delete-space insert", output.last_send, "ball ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, retro_insert_space_bits);
        ok = ok && expect_string("retro insert-space command", output.text, "basket ball ");
        ok = ok && expect_string("retro insert-space delete", output.last_delete, "ball ");
        ok = ok && expect_string("retro insert-space insert", output.last_send, " ball ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("star toggle base word", output.text, "cat ");
        ok = ok && steno_handle_stroke_bits(format_steno, toggle_star_bits);
        ok = ok && expect_string("star toggle translated stroke", output.text, "kitty ");
        ok = ok && expect_string("star toggle delete", output.last_delete, "cat ");
        ok = ok && expect_string("star toggle insert", output.last_send, "kitty ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, period_bits);
        ok = ok && expect_string("period attaches and sets capitalization", output.text, "cat. ");
        ok = ok && expect_string("period delete", output.last_delete, " ");
        ok = ok && expect_string("period insert", output.last_send, ". ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("capitalization after period", output.text, "cat. Cat ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, comma_bits);
        ok = ok && expect_string("comma attaches without capitalization", output.text, "cat, ");
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("comma leaves next word lower-case", output.text, "cat, cat ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cap_next_bits);
        ok = ok && expect_string("case command emits nothing", output.text, "");
        ok = ok && expect_size("case command send count", output.send_count, 0);
        ok = ok && expect_size("case command delete count", output.delete_count, 0);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("cap next word", output.text, "Cat ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, upper_next_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("upper next word", output.text, "CAT ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, lower_next_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, plover_bits);
        ok = ok && expect_string("lower next word", output.text, "plover ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, plover_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, lower_previous_bits);
        ok = ok && expect_string("retro lower previous word", output.text, "plover ");
        ok = ok && expect_string("retro lower delete", output.last_delete, "Plover ");
        ok = ok && expect_string("retro lower insert", output.last_send, "plover ");

        steno_destroy(format_steno);
    }

    Steno *mode_steno = steno_create(&config);
    ok = ok && expect_size("mode steno created", mode_steno != NULL ? 1 : 0, 1);
    if (mode_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, caps_mode_bits);
        ok = ok && expect_string("caps mode emits nothing", output.text, "");
        ok = ok && expect_size("caps mode send count", output.send_count, 0);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("caps mode word", output.text, "CAT ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("reset mode word", output.text, "cat ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, lower_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, plover_bits);
        ok = ok && expect_string("lower mode word", output.text, "plover ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, title_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("title mode word", output.text, "Cat ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, snake_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("snake mode spacing", output.text, "cat_ball_");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, empty_space_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("empty set_space mode", output.text, "catball");
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_space_mode_bits);

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, camel_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("camel mode spacing and case", output.text, "catBall");

        steno_destroy(mode_steno);
    }

    Steno *key_combo_steno = steno_create(&config);
    ok = ok && expect_size("key combo steno created", key_combo_steno != NULL ? 1 : 0, 1);
    if (key_combo_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(key_combo_steno, right_arrow_bits);
        ok = ok && expect_string("key combo command emits no text", output.text, "");
        ok = ok && expect_size("key combo command count", output.key_combo_count, 1);
        ok = ok && expect_string("key combo command", output.last_key_combo, "Right");
        steno_destroy(key_combo_steno);
    }

    Steno *digit_steno = steno_create(&config);
    ok = ok && digit_steno != NULL;
    if (digit_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(digit_steno, one_bits);
        ok = ok && expect_string("digit glue first", output.text, "1 ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(digit_steno, two_bits);
        ok = ok && expect_string("digit glue second", output.text, "12 ");
        ok = ok && expect_string("digit glue delete", output.last_delete, " ");
        ok = ok && expect_string("digit glue insert", output.last_send, "2 ");

        steno_destroy(digit_steno);
    }

    Steno *glue_steno = steno_create(&config);
    ok = ok && glue_steno != NULL;
    if (glue_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(glue_steno, glue_p_bits);
        ok = ok && expect_string("explicit glue first", output.text, "P ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(glue_steno, glue_p_bits);
        ok = ok && expect_string("explicit glue second", output.text, "PP ");
        ok = ok && expect_string("explicit glue delete", output.last_delete, " ");
        ok = ok && expect_string("explicit glue insert", output.last_send, "P ");

        steno_destroy(glue_steno);
    }

    Steno *repeat_steno = steno_create(&config);
    ok = ok && repeat_steno != NULL;
    if (repeat_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(repeat_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(repeat_steno, repeat_bits);
        ok = ok && expect_string("repeat last translation", output.text, "cat cat ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(repeat_steno, undo_bits);
        ok = ok && expect_string("undo repeated translation", output.text, "cat ");
        ok = ok && expect_string("undo repeat delete", output.last_delete, "cat ");

        steno_destroy(repeat_steno);
    }

    Steno *stitch_steno = steno_create(&config);
    ok = ok && stitch_steno != NULL;
    if (stitch_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_a_bits);
        ok = ok && expect_string("stitch first letter", output.text, "A ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_b_bits);
        ok = ok && expect_string("stitch second letter", output.text, "A-B ");
        ok = ok && expect_string("stitch second delete", output.last_delete, " ");
        ok = ok && expect_string("stitch second insert", output.last_send, "-B ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_c_bits);
        ok = ok && expect_string("stitch third letter", output.text, "A-B-C ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, test_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word", output.text, "t-e-s-t ");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, eye_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, to_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word first command", output.text, "eye t-o ");
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word superseded command", output.text, "e-y-e t-o ");

        steno_destroy(stitch_steno);
    }

    const char *layered_paths[] = {
        "tests/test-dictionary.json",
        "tests/test-modal-dictionary.json",
        "tests/test-custom-dictionary.json",
    };
    const bool layered_enabled[] = {
        true,
        false,
        true,
    };
    Steno_Config layered_config = config;
    layered_config.dictionary_path = NULL;
    layered_config.dictionary_paths = layered_paths;
    layered_config.dictionary_enabled = layered_enabled;
    layered_config.dictionary_path_count = sizeof(layered_paths) / sizeof(layered_paths[0]);
    Steno *layered_steno = steno_create(&layered_config);
    ok = ok && layered_steno != NULL;
    if (layered_steno != NULL) {
        const char *kitten = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "KAT", &kitten);
        ok = ok && expect_string("dictionary override", kitten, "kitten");

        const char *modal_off_undo = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_off_undo);
        ok = ok && expect_string("disabled modal dictionary does not override", modal_off_undo, "=undo");

        const char *modal_toggle = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "STPH", &modal_toggle);
        ok = ok && expect_string("custom modal toggle command", modal_toggle, "{plover:toggle_dict:!test-modal-dictionary.json}");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(layered_steno, modal_toggle_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, undo_bits);
        ok = ok && expect_string("enabled modal movement emits no text", output.text, "");
        ok = ok && expect_size("enabled modal movement key combo count", output.key_combo_count, 1);
        ok = ok && expect_string("enabled modal movement key combo", output.last_key_combo, "Left");

        const char *modal_on_left = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_on_left);
        ok = ok && expect_string("enabled modal dictionary overrides", modal_on_left, "{#Left}{^}");

        ok = ok && steno_handle_stroke_bits(layered_steno, modal_toggle_bits);
        const char *modal_off_again = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_off_again);
        ok = ok && expect_string("modal dictionary toggles off", modal_off_again, "=undo");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(layered_steno, eye_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, hyphen_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, to_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, hyphen_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, eye_bits);
        ok = ok && expect_string("hyphen command between words", output.text, "eye-to-eye ");

        steno_destroy(layered_steno);
    }

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "q", true);
    ok = ok && send_key_event(steno, "q", false);
    ok = ok && expect_string("raw # chord", output.text, "# ");

    clear_test_output(&output);
    steno_set_session_active(steno, false);
    ok = ok && !send_key_event(steno, "u", true);
    ok = ok && !send_key_event(steno, "u", false);
    ok = ok && !steno_handle_stroke_bits(steno, gemini_bits);
    ok = ok && expect_string("inactive session suppresses output", output.text, "");
    steno_set_session_active(steno, true);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("active session resumes output", output.text, "fee ");

    const char *star_keys[] = { "t", "g", "b", "y", "h", "n" };
    for (size_t i = 0; i < sizeof(star_keys) / sizeof(star_keys[0]); ++i) {
        clear_test_output(&output);
        ok = ok && send_key_event(steno, star_keys[i], true);
        ok = ok && send_key_event(steno, star_keys[i], false);
        ok = ok && expect_string("star key mapping", output.text, "* ");
    }

    Steno_Config empty_config = config;
    empty_config.dictionary_path = "tests/empty-dictionary.json";
    Steno *empty_steno = steno_create(&empty_config);
    ok = ok && empty_steno != NULL;
    if (empty_steno != NULL) {
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "u", true);
        ok = ok && send_key_event(empty_steno, "u", false);
        ok = ok && expect_string("empty dictionary raw chord", output.text, "F ");

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(empty_steno, gemini_bits);
        ok = ok && expect_string("Gemini PR raw stroke", output.text, "#*Z ");

        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "z", true);
        ok = ok && send_key_event(empty_steno, "z", false);
        ok = ok && expect_string("left multi-bit key", output.text, "#S ");

        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "m", true);
        ok = ok && send_key_event(empty_steno, "m", false);
        ok = ok && expect_string("right multi-bit key implicit hyphen", output.text, "FR ");

        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "comma", true);
        ok = ok && send_key_event(empty_steno, "comma", false);
        ok = ok && expect_string("right multi-bit key explicit hyphen", output.text, "-PB ");

        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "a", true);
        ok = ok && send_key_event(empty_steno, "space", true);
        ok = ok && send_key_event(empty_steno, "i", true);
        ok = ok && send_key_event(empty_steno, "a", false);
        ok = ok && send_key_event(empty_steno, "space", false);
        ok = ok && send_key_event(empty_steno, "i", false);
        ok = ok && expect_string("empty dictionary raw drill chord", output.text, "SAP ");
        steno_destroy(empty_steno);
    }

    Steno_Config gemini_empty_config = empty_config;
    gemini_empty_config.keymap_path = NULL;
    Steno *gemini_empty_steno = steno_create(&gemini_empty_config);
    ok = ok && gemini_empty_steno != NULL;
    if (gemini_empty_steno != NULL) {
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(gemini_empty_steno, gemini_bits);
        ok = ok && expect_string("Gemini PR no-keymap raw stroke", output.text, "#*Z ");
        steno_destroy(gemini_empty_steno);
    }

    arrfree(output.text);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
