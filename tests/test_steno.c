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

int main(void)
{
    Test_Output output = {0};
    Steno_Config config = {
        .keymap_path = "stoin.keymap",
        .dictionary_path = "stoin-dictionary.json",
        .send_text = test_send_text,
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

    ok = ok && send_key_event(steno, "a", true);
    ok = ok && send_key_event(steno, "a", false);
    ok = ok && expect_string("raw # chord", output.text, "# ");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "g", true);
    ok = ok && send_key_event(steno, "g", false);
    ok = ok && expect_string("star key mapping", output.text, "* ");

    const char *the = NULL;
    ok = ok && steno_lookup_stroke(steno, "-T", &the);
    ok = ok && expect_string("dictionary lookup -T", the, "the");

    arrfree(output.text);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
