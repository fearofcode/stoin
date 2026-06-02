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

    const char *the = NULL;
    ok = ok && steno_lookup_stroke(steno, "-T", &the);
    ok = ok && expect_string("dictionary lookup -T", the, "the");

    const char *undo = NULL;
    ok = ok && steno_lookup_stroke(steno, "-R", &undo);
    ok = ok && expect_string("dictionary lookup -R", undo, "=undo");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("first undoable translation", output.text, "fee ");
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "k", false);
    ok = ok && expect_string("second undoable translation", output.text, "fee pay ");
    ok = ok && send_key_event(steno, "m", true);
    ok = ok && send_key_event(steno, "m", false);
    ok = ok && expect_string("one level undo", output.text, "fee ");
    ok = ok && send_key_event(steno, "m", true);
    ok = ok && send_key_event(steno, "m", false);
    ok = ok && expect_string("two level undo", output.text, "");
    ok = ok && send_key_event(steno, "m", true);
    ok = ok && send_key_event(steno, "m", false);
    ok = ok && expect_string("empty undo stack", output.text, "");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "f", true);
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "f", false);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("unicode undoable translation", output.text, "caffè ");
    ok = ok && send_key_event(steno, "m", true);
    ok = ok && send_key_event(steno, "m", false);
    ok = ok && expect_string("unicode undo", output.text, "");

    ok = ok && send_key_event(steno, "a", true);
    ok = ok && send_key_event(steno, "a", false);
    ok = ok && expect_string("raw # chord", output.text, "# ");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "g", true);
    ok = ok && send_key_event(steno, "g", false);
    ok = ok && expect_string("star key mapping", output.text, "* ");

    Steno_Config empty_config = config;
    empty_config.dictionary_path = "tests/empty-dictionary.json";
    Steno *empty_steno = steno_create(&empty_config);
    ok = ok && empty_steno != NULL;
    if (empty_steno != NULL) {
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "j", true);
        ok = ok && send_key_event(empty_steno, "j", false);
        ok = ok && expect_string("empty dictionary raw chord", output.text, "F ");

        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "z", true);
        ok = ok && send_key_event(empty_steno, "space", true);
        ok = ok && send_key_event(empty_steno, "k", true);
        ok = ok && send_key_event(empty_steno, "z", false);
        ok = ok && send_key_event(empty_steno, "space", false);
        ok = ok && send_key_event(empty_steno, "k", false);
        ok = ok && expect_string("empty dictionary raw drill chord", output.text, "SAP ");
        steno_destroy(empty_steno);
    }

    arrfree(output.text);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
