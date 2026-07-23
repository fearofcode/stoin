#include "test_support.h"

#include "steno_stroke.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct Watch_Test {
    Steno *steno;
    size_t reload_count;
} Watch_Test;

bool test_phrasing_tail_filters(void);
bool test_phrasing_starter_filters(void);

static void test_dictionary_watch_callback(void *userdata)
{
    Watch_Test *watch = userdata;
    if (watch == NULL) {
        return;
    }
    if (steno_reload_dictionary_if_changed(watch->steno)) {
        ++watch->reload_count;
    }
}

static void test_phrasing_watch_callback(void *userdata)
{
    Watch_Test *watch = userdata;
    if (watch == NULL) {
        return;
    }
    if (steno_reload_phrasing_if_changed(watch->steno)) {
        ++watch->reload_count;
    }
}

bool test_dictionary_runtime(void)
{
    Test_Output output = {0};
    Steno_Config config = test_steno_config(&output);
    Steno *steno = steno_create(&config);
    if (steno == NULL) {
        fputs("test failed: could not create dictionary runtime steno engine\n", stderr);
        test_output_destroy(&output);
        return false;
    }

    bool ok = true;

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

    const char *phrase_lookup = NULL;
    ok = ok && steno_lookup_stroke(steno, "PW-B", &phrase_lookup);
    ok = ok && expect_string("dictionary lookup remains available on phrase miss", phrase_lookup, "dictionary is a");

    const char *soft_phrase_lookup = NULL;
    ok = ok && steno_lookup_stroke(steno, "TWRF", &soft_phrase_lookup);
    ok = ok && expect_string(
        "ordinary lookup keeps plain multiword dictionary entry",
        soft_phrase_lookup,
        "dictionary as if");

    const char *word_collision_lookup = NULL;
    ok = ok && steno_lookup_stroke(steno, "SKPWOP", &word_collision_lookup);
    ok = ok && expect_string("single dictionary word wins phrase-shaped lookup", word_collision_lookup, "sit");

    const char *have_phrase_lookup = NULL;
    ok = ok && !steno_lookup_stroke(steno, "TKPO-BD", &have_phrase_lookup);

    const char *ampersand = NULL;
    ok = ok && steno_lookup_stroke(steno, "PH", &ampersand);
    ok = ok && expect_string("dictionary lookup unicode escaped key and value", ampersand, "&");

    const char *stories = NULL;
    ok = ok && steno_lookup_stroke(steno, "STOE-R/-Z", &stories);
    ok = ok && expect_string("dictionary lookup canonical multi-stroke", stories, "stories");

    const char *questioningly = NULL;
    ok = ok && steno_lookup_stroke(steno, "#*-678G", &questioningly);
    ok = ok && expect_string("dictionary lookup number-bar digits", questioningly, "the questioningly");

    const char *evergrande = NULL;
    ok = ok && steno_lookup_stroke(steno, "#*-6R/TKPWRA-PBD", &evergrande);
    ok = ok && expect_string("dictionary lookup multi-stroke number-bar digits", evergrande, "the Evergrande");

    const char *internal_hyphen = NULL;
    ok = ok && steno_lookup_stroke(steno, "TKAEURB", &internal_hyphen);
    ok = ok && expect_string(
        "dictionary lookup accepts Plover internal hyphen",
        internal_hyphen,
        "internal hyphen");

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
        ok = ok && expect_string("hot reload initial dictionary", output.text, "old");

        ok = ok && write_text_file(reload_path, "{");
        ok = ok && !steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload keeps old dictionary on parse failure", output.text, " old");

        ok = ok && write_text_file(reload_path, "{ \"S\": \"newer\" }\n");
        ok = ok && steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload updated dictionary", output.text, " newer");

#if !defined(_WIN32)
        ok = ok && write_text_file(reload_path, "{");
        const pid_t writer_pid = fork();
        if (writer_pid == 0) {
            platform_sleep_ms(25);
            _exit(write_text_file(reload_path, "{ \"S\": \"debounced\" }\n") ? 0 : 1);
        }
        ok = ok && writer_pid > 0;
        bool debounce_reloaded = false;
        int writer_status = 0;
        pid_t waited_pid = -1;
        if (writer_pid > 0) {
            debounce_reloaded = steno_reload_dictionary_if_changed(reload_steno);
            waited_pid = waitpid(writer_pid, &writer_status, 0);
        }
        ok = ok && debounce_reloaded;
        ok = ok && waited_pid == writer_pid;
        ok = ok && WIFEXITED(writer_status) && WEXITSTATUS(writer_status) == 0;
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload waits for partial dictionary writes", output.text, " debounced");
#endif

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
        ok = ok && expect_string("platform dictionary watcher reload", output.text, " watched");

        const char *reload_phrasing_path = "build/test-hot-reload-phrasing.json";
        const char *phrasing_is =
            "{\n"
            "  \"initial_verbs\": {\n"
            "    \"tails\": [{\"id\": \"a\", \"stroke\": \"-B\", \"text\": \"a\"}],\n"
            "    \"stems\": [{\"stroke\": \"SKPO\", \"forms\": [{\"stroke\": \"\", \"text\": \"is\"}]}]\n"
            "  },\n"
            "  \"final_verbs\": {\n"
            "    \"contraction_stroke\": \"U\",\n"
            "    \"starters\": [],\n"
            "    \"operators\": [],\n"
            "    \"structures\": [],\n"
            "    \"verbs\": [],\n"
            "    \"enders\": []\n"
            "  }\n"
            "}\n";
        const char *phrasing_was =
            "{\n"
            "  \"initial_verbs\": {\n"
            "    \"tails\": [{\"id\": \"a\", \"stroke\": \"-B\", \"text\": \"a\"}],\n"
            "    \"stems\": [{\"stroke\": \"SKPO\", \"forms\": [{\"stroke\": \"\", \"text\": \"was\"}]}]\n"
            "  },\n"
            "  \"final_verbs\": {\n"
            "    \"contraction_stroke\": \"U\",\n"
            "    \"starters\": [],\n"
            "    \"operators\": [],\n"
            "    \"structures\": [],\n"
            "    \"verbs\": [],\n"
            "    \"enders\": []\n"
            "  }\n"
            "}\n";
        const char *phrasing_are =
            "{\n"
            "  \"initial_verbs\": {\n"
            "    \"tails\": [{\"id\": \"a\", \"stroke\": \"-B\", \"text\": \"a\"}],\n"
            "    \"stems\": [{\"stroke\": \"SKPO\", \"forms\": [{\"stroke\": \"\", \"text\": \"are\"}]}]\n"
            "  },\n"
            "  \"final_verbs\": {\n"
            "    \"contraction_stroke\": \"U\",\n"
            "    \"starters\": [],\n"
            "    \"operators\": [],\n"
            "    \"structures\": [],\n"
            "    \"verbs\": [],\n"
            "    \"enders\": []\n"
            "  }\n"
            "}\n";
        const char *phrasing_duplicate_iv_tail =
            "{\n"
            "  \"initial_verbs\": {\n"
            "    \"tails\": [\n"
            "      {\"id\": \"a\", \"stroke\": \"-B\", \"text\": \"a\"},\n"
            "      {\"id\": \"the\", \"stroke\": \"-B\", \"text\": \"the\"}\n"
            "    ],\n"
            "    \"stems\": [{\"stroke\": \"SKPO\", \"forms\": [{\"stroke\": \"\", \"text\": \"is\"}]}]\n"
            "  },\n"
            "  \"final_verbs\": {\n"
            "    \"contraction_stroke\": \"U\",\n"
            "    \"starters\": [],\n"
            "    \"operators\": [],\n"
            "    \"structures\": [],\n"
            "    \"verbs\": [],\n"
            "    \"enders\": []\n"
            "  }\n"
            "}\n";
        ok = ok && write_text_file(reload_phrasing_path, phrasing_is);
        Steno_Config phrasing_reload_config = config;
        phrasing_reload_config.phrasing_path = reload_phrasing_path;
        Steno *phrasing_reload_steno = steno_create(&phrasing_reload_config);
        ok = ok && phrasing_reload_steno != NULL;
        if (phrasing_reload_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_phrase_test_stroke(phrasing_reload_steno, "SKPO-B");
            ok = ok && expect_string("hot reload initial phrasing", output.text, "is a");

            ok = ok && write_text_file(reload_phrasing_path, "{");
            ok = ok && !steno_reload_phrasing(phrasing_reload_steno);
            clear_test_output(&output);
            ok = ok && handle_phrase_test_stroke(phrasing_reload_steno, "SKPO-B");
            ok = ok && expect_string("hot reload keeps old phrasing on parse failure", output.text, " is a");

            ok = ok && write_text_file(reload_phrasing_path, phrasing_duplicate_iv_tail);
            ok = ok && !steno_reload_phrasing(phrasing_reload_steno);
            clear_test_output(&output);
            ok = ok && handle_phrase_test_stroke(phrasing_reload_steno, "SKPO-B");
            ok = ok && expect_string("hot reload keeps old phrasing on duplicate stroke", output.text, " is a");

            ok = ok && write_text_file(reload_phrasing_path, phrasing_was);
            ok = ok && steno_reload_phrasing(phrasing_reload_steno);
            clear_test_output(&output);
            ok = ok && handle_phrase_test_stroke(phrasing_reload_steno, "SKPO-B");
            ok = ok && expect_string("hot reload updated phrasing", output.text, " was a");

            Watch_Test phrase_watch = {
                .steno = phrasing_reload_steno,
            };
            const char *const phrase_watch_paths[] = { reload_phrasing_path };
            ok = ok && platform_file_watcher_start(
                phrase_watch_paths,
                sizeof(phrase_watch_paths) / sizeof(phrase_watch_paths[0]),
                test_phrasing_watch_callback,
                &phrase_watch
            );
            ok = ok && write_text_file(reload_phrasing_path, phrasing_are);
            for (size_t attempt = 0; ok && phrase_watch.reload_count == 0 && attempt < 50; ++attempt) {
                platform_file_watcher_poll();
                platform_sleep_ms(10);
            }
            platform_file_watcher_stop();
            ok = ok && phrase_watch.reload_count > 0;
            clear_test_output(&output);
            ok = ok && handle_phrase_test_stroke(phrasing_reload_steno, "SKPO-B");
            ok = ok && expect_string("platform phrasing watcher reload", output.text, " are a");
            steno_destroy(phrasing_reload_steno);
        }
        remove(reload_phrasing_path);

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
        ok = ok && expect_string("hot reload while disabled applies after reenable", output.text, " disabled");

        steno_destroy(reload_steno);
    }
    remove(reload_path);

    const char *dump_path = "build/test-dictionary-dump.json";
    ok = ok && steno_dump_dictionary_json(steno, dump_path);
    size_t dump_size = 0;
    char *dump = read_entire_file(dump_path, &dump_size);
    ok = ok && dump != NULL && dump_size > 0;
    if (dump != NULL) {
        ok = ok && strstr(dump, "\"STOER/Z\"") != NULL && strstr(dump, "\"stories\"") != NULL;
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
            uint64_t trace_bits = 0;
            uint64_t trace_cat_bits = 0;
            uint64_t trace_toggle_star_bits = 0;
            clear_test_output(&output);
            ok = ok && stroke_string_to_bits("-T", &trace_bits);
            ok = ok && stroke_string_to_bits("KAT", &trace_cat_bits);
            ok = ok && stroke_string_to_bits("#*", &trace_toggle_star_bits);
            ok = ok && steno_handle_stroke_bits(trace_steno, trace_bits);
            ok = ok && handle_phrase_test_stroke(trace_steno, "SKPWO-B");
            ok = ok && handle_test_stroke(trace_steno, "#KW");
            ok = ok && handle_test_stroke(trace_steno, "SAO");
            ok = ok && steno_handle_stroke_bits(trace_steno, trace_cat_bits);
            ok = ok && steno_handle_stroke(trace_steno, ((Stroke_Input) {
                .bits = trace_toggle_star_bits,
            }));
            ok = ok && expect_trace_contains(trace_file, "trace translated stroke", "-T -> the\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace phrase stroke",
                "SKPWOB [phrase] -> is a\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace dictionary stroke with number bar",
                "#* -> {*}\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace dictionary stroke",
                "#KW -> test\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace raw stroke",
                "SAO -> [untranslated]\n");
            steno_destroy(trace_steno);
        }
        fclose(trace_file);
    }

    FILE *disabled_suggestions_file = tmpfile();
    ok = ok && disabled_suggestions_file != NULL;
    if (disabled_suggestions_file != NULL) {
        Steno_Config disabled_suggestions_config = config;
        disabled_suggestions_config.suggestions_file = disabled_suggestions_file;
        Steno *disabled_suggestions_steno = steno_create(&disabled_suggestions_config);
        ok = ok && disabled_suggestions_steno != NULL;
        if (disabled_suggestions_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_test_stroke(disabled_suggestions_steno, "TPH");
            ok = ok && handle_test_stroke(disabled_suggestions_steno, "-T");
            ok = ok && expect_file_not_contains(
                disabled_suggestions_file,
                "brevity suggestions disabled",
                "Suggestion:");
            steno_destroy(disabled_suggestions_steno);
        }
        fclose(disabled_suggestions_file);
    }

    FILE *suggestions_file = tmpfile();
    ok = ok && suggestions_file != NULL;
    if (suggestions_file != NULL) {
        Steno_Config suggestions_config = config;
        suggestions_config.suggestions_file = suggestions_file;
        suggestions_config.print_suggestions = true;
        Steno *suggestions_steno = steno_create(&suggestions_config);
        ok = ok && suggestions_steno != NULL;
        if (suggestions_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_test_stroke(suggestions_steno, "KAT");
            ok = ok && handle_test_stroke(suggestions_steno, "TPH");
            ok = ok && handle_test_stroke(suggestions_steno, "-T");
            ok = ok && expect_file_contains(
                suggestions_file,
                "brevity suggests shorter phrase",
                "Suggestion: Use TPH-T for \"in the\"\n");
            ok = ok && handle_test_stroke(suggestions_steno, "PW-G");
            ok = ok && expect_file_contains(
                suggestions_file,
                "brevity prefers longer phrase",
                "Suggestion: Use TPH-T/PWG for \"in the beginning\"\n");
            ok = ok && expect_string(
                "brevity suggestions do not change output",
                output.text,
                "cat in the beginning");
            steno_destroy(suggestions_steno);
        }
        fclose(suggestions_file);
    }

    FILE *suffix_suggestions_file = tmpfile();
    ok = ok && suffix_suggestions_file != NULL;
    if (suffix_suggestions_file != NULL) {
        Steno_Config suffix_suggestions_config = config;
        suffix_suggestions_config.suggestions_file = suffix_suggestions_file;
        suffix_suggestions_config.print_suggestions = true;
        Steno *suffix_suggestions_steno = steno_create(&suffix_suggestions_config);
        ok = ok && suffix_suggestions_steno != NULL;
        if (suffix_suggestions_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_test_stroke(suffix_suggestions_steno, "KWEUBG");
            ok = ok && handle_test_stroke(suffix_suggestions_steno, "-L");
            ok = ok && expect_file_contains(
                suffix_suggestions_file,
                "brevity suggests collapsed attached suffix",
                "Suggestion: Use KWEUL for \"quickly\"\n");
            ok = ok && expect_string(
                "collapsed attached suffix output",
                output.text,
                "quickly");
            steno_destroy(suffix_suggestions_steno);
        }
        fclose(suffix_suggestions_file);
    }

    FILE *brief_suggestions_file = tmpfile();
    ok = ok && brief_suggestions_file != NULL;
    if (brief_suggestions_file != NULL) {
        Steno_Config brief_suggestions_config = config;
        brief_suggestions_config.suggestions_file = brief_suggestions_file;
        brief_suggestions_config.print_suggestions = true;
        Steno *brief_suggestions_steno = steno_create(&brief_suggestions_config);
        ok = ok && brief_suggestions_steno != NULL;
        if (brief_suggestions_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_test_stroke(brief_suggestions_steno, "TPH-T");
            ok = ok && expect_string("brief suggestion direct output", output.text, "in the");
            ok = ok && expect_file_not_contains(
                brief_suggestions_file,
                "brevity does not suggest typed brief",
                "Suggestion:");
            steno_destroy(brief_suggestions_steno);
        }
        fclose(brief_suggestions_file);
    }

    FILE *suggestion_log_file = tmpfile();
    FILE *silent_suggestions_file = tmpfile();
    ok = ok && suggestion_log_file != NULL && silent_suggestions_file != NULL;
    if (suggestion_log_file != NULL && silent_suggestions_file != NULL) {
        Steno_Config suggestion_log_config = config;
        suggestion_log_config.suggestions_file = silent_suggestions_file;
        suggestion_log_config.suggestion_log_file = suggestion_log_file;
        Steno *suggestion_log_steno = steno_create(&suggestion_log_config);
        ok = ok && suggestion_log_steno != NULL;
        if (suggestion_log_steno != NULL) {
            clear_test_output(&output);
            ok = ok && handle_test_stroke(suggestion_log_steno, "TPH");
            ok = ok && handle_test_stroke(suggestion_log_steno, "-T");
            ok = ok && expect_file_not_contains(
                silent_suggestions_file,
                "suggestion log does not print",
                "Suggestion:");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log suggested outline",
                "\"suggested_outline\":\"TPH-T\"");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log typed outline",
                "\"typed_outline\":\"TPH/-T\"");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log text",
                "\"text\":\"in the\"");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log typed strokes",
                "\"typed_strokes\":2");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log suggested strokes",
                "\"suggested_strokes\":1");
            ok = ok && expect_file_contains(
                suggestion_log_file,
                "suggestion log saved strokes",
                "\"saved_strokes\":1");
            steno_destroy(suggestion_log_steno);
        }
    }
    if (silent_suggestions_file != NULL) {
        fclose(silent_suggestions_file);
    }
    if (suggestion_log_file != NULL) {
        fclose(suggestion_log_file);
    }


    const bool phrasing_tail_filters_ok = test_phrasing_tail_filters();
    ok = phrasing_tail_filters_ok && ok;
    const bool phrasing_starter_filters_ok = test_phrasing_starter_filters();
    ok = phrasing_starter_filters_ok && ok;

    steno_destroy(steno);
    test_output_destroy(&output);
    return ok;
}
