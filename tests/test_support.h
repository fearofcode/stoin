#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include "orthography.h"
#include "platform.h"
#include "steno.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Test_Output {
    char *text;
    char last_send[128];
    char last_delete[128];
    char last_key_combo[128];
    size_t send_count;
    size_t delete_count;
    size_t key_combo_count;
    bool fail_next_send;
} Test_Output;

enum {
    TEST_KEYCODE_ESCAPE = 53,
    TEST_KEYCODE_LEFT_CONTROL = 59,
};

Steno_Config test_steno_config(Test_Output *output);
void test_output_destroy(Test_Output *output);

bool test_send_text(const char *utf8, void *userdata);
bool test_delete_text(const char *utf8, void *userdata);
bool test_send_key_combination(const char *combo, void *userdata);

void clear_test_output(Test_Output *output);
void reset_output_log(Test_Output *output);

bool reset_test_steno(Steno **steno, const Steno_Config *config);
bool handle_test_stroke(Steno *steno, const char *stroke);
bool handle_phrase_test_stroke(Steno *steno, const char *stroke);
bool expect_stroke_output(
    Steno **steno,
    const Steno_Config *config,
    Test_Output *output,
    const char *name,
    const char *stroke,
    const char *expected
);
bool expect_phrase_stroke_output(
    Steno **steno,
    const Steno_Config *config,
    Test_Output *output,
    const char *name,
    const char *stroke,
    const char *expected
);

bool write_text_file(const char *path, const char *contents);
bool send_key_event(Steno *steno, const char *key_name, bool is_down);
bool send_raw_key_event(
    Steno *steno,
    uint16_t keycode,
    bool is_down,
    bool control,
    bool option,
    bool command
);

bool expect_string(const char *name, const char *actual, const char *expected);
bool expect_size_at_most(const char *name, size_t actual, size_t expected_max);
bool expect_size(const char *name, size_t actual, size_t expected);
bool expect_u64(const char *name, uint64_t actual, uint64_t expected);
bool expect_bytes(
    const char *name,
    const uint8_t *actual,
    size_t actual_size,
    const uint8_t *expected,
    size_t expected_size
);
bool expect_stroke_format(const char *input, const char *expected);
bool expect_orthography(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    const char *expected
);
bool read_file_contents(FILE *file, char *contents, size_t contents_size);
bool expect_file_contains(FILE *file, const char *name, const char *expected);
bool expect_file_not_contains(FILE *file, const char *name, const char *unexpected);
bool expect_trace_contains(FILE *trace_file, const char *name, const char *expected);

#endif
