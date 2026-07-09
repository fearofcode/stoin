#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool run_translation_fixture(const char *name, const char *expected, char *outlines)
{
    Test_Output output = {0};
    Steno_Config config = test_steno_config(&output);
    Steno *steno = steno_create(&config);
    bool ok = true;

    if (steno == NULL) {
        fprintf(stderr, "test failed: %s: could not create steno engine\n", name);
        test_output_destroy(&output);
        return false;
    }

    clear_test_output(&output);
    reset_output_log(&output);

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

    const char *actual = output.text == NULL ? "" : output.text;
    ok = expect_string(name, actual, expected) && ok;

    steno_destroy(steno);
    test_output_destroy(&output);
    return ok;
}

bool test_translation_fixtures(void)
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

        char *name = line;
        char *outlines = second_tab + 1;
        ++fixture_count;
        ok = run_translation_fixture(name, expected, outlines) && ok;
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
