#include "gemini_pr.h"
#include "platform.h"
#include "steno.h"
#include "steno_stroke.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Test_Output {
    char *text;
} Test_Output;

static bool test_send_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
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

    const size_t new_length = length - delete_length;
    arrsetlen(output->text, new_length + 1);
    output->text[new_length] = '\0';
    return true;
}

static void clear_test_output(Test_Output *output)
{
    arrsetlen(output->text, 0);
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

int main(void)
{
    Test_Output output = {0};
    Steno_Config config = {
        .keymap_path = "stoin.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .send_text = test_send_text,
        .delete_text = test_delete_text,
        .send_userdata = &output,
    };

    Steno *steno = steno_create(&config);
    if (steno == NULL) {
        fputs("test failed: could not create steno engine\n", stderr);
        return 1;
    }

    bool ok = true;

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

    const char *the = NULL;
    ok = ok && steno_lookup_stroke(steno, "-T", &the);
    ok = ok && expect_string("dictionary lookup -T", the, "the");

    const char *undo = NULL;
    ok = ok && steno_lookup_stroke(steno, "-R", &undo);
    ok = ok && expect_string("dictionary lookup -R", undo, "=undo");

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

    ok = ok && send_key_event(steno, "q", true);
    ok = ok && send_key_event(steno, "q", false);
    ok = ok && expect_string("raw # chord", output.text, "# ");

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
