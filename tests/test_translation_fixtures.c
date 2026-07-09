#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct Translation_Fixture_Output {
    char text[4096];
    char events[8192];
} Translation_Fixture_Output;

static bool fixture_append(char *buffer, size_t buffer_size, const char *text)
{
    const size_t length = strlen(buffer);
    const size_t text_length = strlen(text);
    if (length + text_length >= buffer_size) {
        return false;
    }
    memcpy(buffer + length, text, text_length + 1);
    return true;
}

static bool fixture_append_event(Translation_Fixture_Output *output, const char *prefix, const char *text)
{
    return (output->events[0] == '\0' || fixture_append(output->events, sizeof(output->events), "|"))
        && fixture_append(output->events, sizeof(output->events), prefix)
        && fixture_append(output->events, sizeof(output->events), text);
}

static bool fixture_send_text(const char *utf8, void *userdata)
{
    Translation_Fixture_Output *output = userdata;
    return output != NULL
        && fixture_append(output->text, sizeof(output->text), utf8)
        && fixture_append_event(output, "S:", utf8);
}

static bool fixture_delete_text(const char *utf8, void *userdata)
{
    Translation_Fixture_Output *output = userdata;
    if (output == NULL) {
        return false;
    }

    const size_t delete_length = strlen(utf8);
    const size_t length = strlen(output->text);
    if (delete_length > length || memcmp(output->text + length - delete_length, utf8, delete_length) != 0) {
        return false;
    }

    output->text[length - delete_length] = '\0';
    return fixture_append_event(output, "D:", utf8);
}

static bool fixture_send_key_combination(const char *combo, void *userdata)
{
    Translation_Fixture_Output *output = userdata;
    return output != NULL && fixture_append_event(output, "K:", combo);
}

static bool run_translation_fixture(
    const char *name,
    const char *expected,
    char *outlines,
    const char *expected_events
)
{
    Translation_Fixture_Output output = {0};
    Steno_Config config = {
        .keymap_path = "tests/test.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .word_list_path = "tests/test-words.txt",
        .phrasing_path = "tests/test-phrasing.json",
        .send_text = fixture_send_text,
        .delete_text = fixture_delete_text,
        .send_key_combination = fixture_send_key_combination,
        .send_userdata = &output,
    };
    Steno *steno = steno_create(&config);
    bool ok = true;

    if (steno == NULL) {
        fprintf(stderr, "test failed: %s: could not create steno engine\n", name);
        return false;
    }

    size_t stroke_count = 0;
    for (char *token = strtok(outlines, " "); token != NULL; token = strtok(NULL, " ")) {
        ++stroke_count;
        if (!handle_test_stroke(steno, token)) {
            ok = false;
            break;
        }
    }

    if (stroke_count == 0) {
        fprintf(stderr, "test failed: %s: fixture has no outlines\n", name);
        ok = false;
    }

    char event_name[256] = {0};
    snprintf(event_name, sizeof(event_name), "%s events", name);
    ok = expect_string(name, output.text, expected) && ok;
    ok = expect_string(event_name, output.events, expected_events) && ok;

    steno_destroy(steno);
    return ok;
}

static bool phrase_mode_from_string(const char *text, Steno_Phrase_Mode *out_mode)
{
    if (strcmp(text, "all") == 0) {
        *out_mode = STENO_PHRASE_MODE_ALL;
        return true;
    }
    if (strcmp(text, "verbs") == 0) {
        *out_mode = STENO_PHRASE_MODE_VERBS;
        return true;
    }
    if (strcmp(text, "nonverbs") == 0) {
        *out_mode = STENO_PHRASE_MODE_NONVERBS;
        return true;
    }
    return false;
}

static bool run_phrase_translation_fixture(
    const char *name,
    Steno_Phrase_Mode phrase_mode,
    const char *expected,
    char *outlines,
    const char *expected_events
)
{
    Translation_Fixture_Output output = {0};
    Steno_Config config = {
        .keymap_path = "tests/test.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .word_list_path = "tests/test-words.txt",
        .phrasing_path = "tests/test-phrasing.json",
        .send_text = fixture_send_text,
        .delete_text = fixture_delete_text,
        .send_key_combination = fixture_send_key_combination,
        .send_userdata = &output,
    };
    Steno *steno = steno_create(&config);
    bool ok = true;

    if (steno == NULL) {
        fprintf(stderr, "test failed: %s: could not create steno engine\n", name);
        return false;
    }

    size_t stroke_count = 0;
    for (char *token = strtok(outlines, " "); token != NULL; token = strtok(NULL, " ")) {
        ++stroke_count;
        if (!handle_phrase_test_stroke(steno, token, phrase_mode)) {
            ok = false;
            break;
        }
    }

    if (stroke_count == 0) {
        fprintf(stderr, "test failed: %s: fixture has no outlines\n", name);
        ok = false;
    }

    char event_name[256] = {0};
    snprintf(event_name, sizeof(event_name), "%s events", name);
    ok = expect_string(name, output.text, expected) && ok;
    ok = expect_string(event_name, output.events, expected_events) && ok;

    steno_destroy(steno);
    return ok;
}

static bool run_translation_fixture_file(void)
{
    FILE *file = fopen("tests/translation-fixtures.tsv", "rb");
    if (file == NULL) {
        fputs("test failed: could not open tests/translation-fixtures.tsv\n", stderr);
        return false;
    }

    bool ok = true;
    size_t fixture_count = 0;
    size_t line_number = 0;
    char line[4096];

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        char *first_tab = strchr(line, '\t');
        if (first_tab == NULL) {
            fprintf(stderr, "test failed: translation fixture line %zu has no expected column\n", line_number);
            ok = false;
            continue;
        }
        *first_tab = '\0';

        char *expected = first_tab + 1;
        char *second_tab = strchr(expected, '\t');
        if (second_tab == NULL) {
            fprintf(stderr, "test failed: translation fixture line %zu has no outline column\n", line_number);
            ok = false;
            continue;
        }
        *second_tab = '\0';

        char *outlines = second_tab + 1;
        char *third_tab = strchr(outlines, '\t');
        if (third_tab == NULL) {
            fprintf(stderr, "test failed: translation fixture line %zu has no event column\n", line_number);
            ok = false;
            continue;
        }
        *third_tab = '\0';

        char *name = line;
        char *expected_events = third_tab + 1;
        ++fixture_count;
        ok = run_translation_fixture(name, expected, outlines, expected_events) && ok;
    }

    if (fixture_count == 0) {
        fputs("test failed: no translation fixtures found\n", stderr);
        ok = false;
    }

    if (fclose(file) != 0) {
        fputs("test failed: could not close tests/translation-fixtures.tsv\n", stderr);
        ok = false;
    }

    return ok;
}

static bool run_phrase_translation_fixture_file(void)
{
    FILE *file = fopen("tests/phrase-translation-fixtures.tsv", "rb");
    if (file == NULL) {
        fputs("test failed: could not open tests/phrase-translation-fixtures.tsv\n", stderr);
        return false;
    }

    bool ok = true;
    size_t fixture_count = 0;
    size_t line_number = 0;
    char line[4096];

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        char *first_tab = strchr(line, '\t');
        if (first_tab == NULL) {
            fprintf(stderr, "test failed: phrase fixture line %zu has no mode column\n", line_number);
            ok = false;
            continue;
        }
        *first_tab = '\0';

        char *mode_text = first_tab + 1;
        char *second_tab = strchr(mode_text, '\t');
        if (second_tab == NULL) {
            fprintf(stderr, "test failed: phrase fixture line %zu has no expected column\n", line_number);
            ok = false;
            continue;
        }
        *second_tab = '\0';

        Steno_Phrase_Mode phrase_mode = STENO_PHRASE_MODE_NONE;
        if (!phrase_mode_from_string(mode_text, &phrase_mode)) {
            fprintf(stderr, "test failed: phrase fixture line %zu has invalid mode '%s'\n", line_number, mode_text);
            ok = false;
            continue;
        }

        char *expected = second_tab + 1;
        char *third_tab = strchr(expected, '\t');
        if (third_tab == NULL) {
            fprintf(stderr, "test failed: phrase fixture line %zu has no outline column\n", line_number);
            ok = false;
            continue;
        }
        *third_tab = '\0';

        char *outlines = third_tab + 1;
        char *fourth_tab = strchr(outlines, '\t');
        if (fourth_tab == NULL) {
            fprintf(stderr, "test failed: phrase fixture line %zu has no event column\n", line_number);
            ok = false;
            continue;
        }
        *fourth_tab = '\0';

        char *name = line;
        char *expected_events = fourth_tab + 1;
        ++fixture_count;
        ok = run_phrase_translation_fixture(name, phrase_mode, expected, outlines, expected_events) && ok;
    }

    if (fixture_count == 0) {
        fputs("test failed: no phrase translation fixtures found\n", stderr);
        ok = false;
    }

    if (fclose(file) != 0) {
        fputs("test failed: could not close tests/phrase-translation-fixtures.tsv\n", stderr);
        ok = false;
    }

    return ok;
}

bool test_translation_fixtures(void)
{
    bool ok = true;
    ok = run_translation_fixture_file() && ok;
    ok = run_phrase_translation_fixture_file() && ok;
    return ok;
}
