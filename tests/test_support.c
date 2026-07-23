#include "test_support.h"

#include "steno_stroke.h"

#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

Steno_Config test_steno_config(Test_Output *output)
{
    return (Steno_Config) {
        .keymap_path = "tests/test.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .word_list_path = "tests/test-words.txt",
        .phrasing_path = "tests/test-phrasing.json",
        .send_text = test_send_text,
        .delete_text = test_delete_text,
        .send_key_combination = test_send_key_combination,
        .send_userdata = output,
    };
}

void test_output_destroy(Test_Output *output)
{
    if (output == NULL) {
        return;
    }
    arrfree(output->text);
    output->text = NULL;
}

bool test_send_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
    ++output->send_count;
    snprintf(output->last_send, sizeof(output->last_send), "%s", utf8);
    if (output->fail_next_send) {
        output->fail_next_send = false;
        return false;
    }

    if (output->text != NULL && arrlenu(output->text) > 0) {
        arrpop(output->text);
    }
    for (const char *p = utf8; *p != '\0'; ++p) {
        arrput(output->text, *p);
    }
    arrput(output->text, '\0');
    return true;
}

bool test_delete_text(const char *utf8, void *userdata)
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

bool test_send_key_combination(const char *combo, void *userdata)
{
    Test_Output *output = userdata;
    ++output->key_combo_count;
    snprintf(output->last_key_combo, sizeof(output->last_key_combo), "%s", combo);
    return true;
}

void clear_test_output(Test_Output *output)
{
    arrsetlen(output->text, 0);
    arrput(output->text, '\0');
}

void reset_output_log(Test_Output *output)
{
    output->last_send[0] = '\0';
    output->last_delete[0] = '\0';
    output->last_key_combo[0] = '\0';
    output->send_count = 0;
    output->delete_count = 0;
    output->key_combo_count = 0;
}

bool reset_test_steno(Steno **steno, const Steno_Config *config)
{
    if (steno == NULL || config == NULL) {
        return false;
    }
    steno_destroy(*steno);
    *steno = steno_create(config);
    return *steno != NULL;
}

bool handle_test_stroke(Steno *steno, const char *stroke)
{
    uint64_t bits = 0;
    if (!stroke_string_to_bits(stroke, &bits)) {
        fprintf(stderr, "test failed: could not parse stroke '%s'\n", stroke);
        return false;
    }

    const bool ok = steno_handle_stroke_bits(steno, bits);
    if (!ok) {
        fprintf(stderr, "test failed: stroke '%s' was not handled\n", stroke);
    }
    return ok;
}

bool handle_phrase_test_stroke(Steno *steno, const char *stroke)
{
    uint64_t bits = 0;
    if (!stroke_string_to_bits(stroke, &bits)) {
        fprintf(stderr, "test failed: could not parse phrase stroke '%s'\n", stroke);
        return false;
    }

    const bool ok = steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = bits,
        .phrase_namespace = PHRASE_NAMESPACE_INITIAL_VERB,
    }));
    if (!ok) {
        fprintf(stderr, "test failed: phrase stroke '%s' was not handled\n", stroke);
    }
    return ok;
}

bool expect_stroke_output(
    Steno **steno,
    const Steno_Config *config,
    Test_Output *output,
    const char *name,
    const char *stroke,
    const char *expected
)
{
    if (!reset_test_steno(steno, config)) {
        return false;
    }
    clear_test_output(output);
    reset_output_log(output);
    return handle_test_stroke(*steno, stroke)
        && expect_string(name, output->text, expected);
}

bool expect_phrase_stroke_output(
    Steno **steno,
    const Steno_Config *config,
    Test_Output *output,
    const char *name,
    const char *stroke,
    const char *expected
)
{
    if (!reset_test_steno(steno, config)) {
        return false;
    }
    clear_test_output(output);
    reset_output_log(output);
    return handle_phrase_test_stroke(*steno, stroke)
        && expect_string(name, output->text, expected);
}

bool write_text_file(const char *path, const char *contents)
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

bool send_key_event(Steno *steno, const char *key_name, bool is_down)
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

bool send_raw_key_event(
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

bool expect_string(const char *name, const char *actual, const char *expected)
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

bool expect_size_at_most(const char *name, size_t actual, size_t expected_max)
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

bool expect_size(const char *name, size_t actual, size_t expected)
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

bool expect_u64(const char *name, uint64_t actual, uint64_t expected)
{
    if (actual == expected) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected 0x%llx, got 0x%llx\n",
        name,
        (unsigned long long)expected,
        (unsigned long long)actual);
    return false;
}

bool expect_bytes(
    const char *name,
    const uint8_t *actual,
    size_t actual_size,
    const uint8_t *expected,
    size_t expected_size
)
{
    if (actual_size == expected_size && memcmp(actual, expected, expected_size) == 0) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected %zu bytes, got %zu bytes\n",
        name,
        expected_size,
        actual_size);
    const size_t min_size = actual_size < expected_size ? actual_size : expected_size;
    for (size_t i = 0; i < min_size; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "test failed: %s: byte %zu expected 0x%02x, got 0x%02x\n",
                name,
                i,
                expected[i],
                actual[i]);
            break;
        }
    }
    return false;
}

bool expect_stroke_format(const char *input, const char *expected)
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

bool expect_orthography(
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

bool read_file_contents(FILE *file, char *contents, size_t contents_size)
{
    if (file == NULL || contents == NULL || contents_size == 0) {
        return false;
    }

    fflush(file);
    rewind(file);
    const size_t read = fread(contents, 1, contents_size - 1, file);
    contents[read] = '\0';
    return true;
}

bool expect_file_contains(FILE *file, const char *name, const char *expected)
{
    char contents[2048] = {0};
    if (!read_file_contents(file, contents, sizeof(contents))) {
        fprintf(stderr, "test failed: %s: could not read file output\n", name);
        return false;
    }
    if (strstr(contents, expected) != NULL) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected output to contain '%s', got '%s'\n",
        name,
        expected,
        contents);
    return false;
}

bool expect_file_not_contains(FILE *file, const char *name, const char *unexpected)
{
    char contents[2048] = {0};
    if (!read_file_contents(file, contents, sizeof(contents))) {
        fprintf(stderr, "test failed: %s: could not read file output\n", name);
        return false;
    }
    if (strstr(contents, unexpected) == NULL) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected output not to contain '%s', got '%s'\n",
        name,
        unexpected,
        contents);
    return false;
}

bool expect_trace_contains(FILE *trace_file, const char *name, const char *expected)
{
    return expect_file_contains(trace_file, name, expected);
}
