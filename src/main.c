#include "gemini_pr.h"
#include "platform.h"
#include "steno.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum Input_Mode {
    INPUT_MODE_QWERTY,
    INPUT_MODE_GEMINI_PR,
} Input_Mode;

typedef struct App {
    Steno *steno;
} App;

static volatile sig_atomic_t g_stop_requested;

static bool send_text(const char *utf8, void *userdata)
{
    (void)userdata;
    return platform_send_text_utf8(utf8);
}

static bool delete_text(const char *utf8, void *userdata)
{
    (void)userdata;
    return platform_delete_text_utf8(utf8);
}

static bool handle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;
    return steno_handle_event(app->steno, event);
}

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

static void print_usage(void)
{
    fputs("usage: stoin [--input gemini-pr|qwerty] [--gemini-port PATH] [--gemini-baud BAUD]\n", stderr);
    fputs("       stoin --lookup STROKE\n", stderr);
    fputs("       stoin --dump-dictionary [PATH]\n", stderr);
}

static bool parse_baud_rate(const char *value, int *out_baud_rate)
{
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (value == end || *end != '\0' || parsed <= 0 || parsed > 1000000) {
        return false;
    }
    *out_baud_rate = (int)parsed;
    return true;
}

static int run_qwerty(App *app)
{
    if (!platform_init(handle_input, app)) {
        return 1;
    }

    puts("stoin: macOS qwerty event tap running");
    printf("stoin: loaded %zu key bindings and %zu dictionary entries\n",
        steno_key_binding_count(app->steno),
        steno_dictionary_count(app->steno));
    puts("stoin: steno capture starts enabled; press Ctrl+Esc to toggle it");
    puts("stoin: Command/Control/Option shortcuts pass through; press Ctrl+C in this terminal to quit");

    platform_run();
    platform_shutdown();
    return 0;
}

static int run_gemini_pr(App *app, const char *port_path, int baud_rate)
{
    Gemini_Pr gemini;
    const Gemini_Pr_Config gemini_config = {
        .port_path = port_path,
        .baud_rate = baud_rate,
    };
    if (!gemini_pr_open(&gemini, &gemini_config)) {
        return 1;
    }

    if (!platform_output_init()) {
        gemini_pr_close(&gemini);
        return 1;
    }

    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    printf("stoin: Gemini PR serial capture running on %s at %d baud 8N1\n",
        gemini_pr_port_path(&gemini),
        baud_rate == 0 ? GEMINI_PR_DEFAULT_BAUD_RATE : baud_rate);
    printf("stoin: loaded %zu dictionary entries\n", steno_dictionary_count(app->steno));
    puts("stoin: press Ctrl+C in this terminal to quit");

    while (!g_stop_requested && !gemini_pr_had_error(&gemini)) {
        uint64_t stroke_bits = 0;
        if (gemini_pr_read_stroke(&gemini, &stroke_bits)) {
            (void)steno_handle_stroke_bits(app->steno, stroke_bits);
        }
    }

    gemini_pr_close(&gemini);
    platform_shutdown();
    return gemini_pr_had_error(&gemini) ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--lookup") == 0) {
        const Steno_Config steno_config = {
            .keymap_path = NULL,
            .dictionary_path = "stoin-dictionary.json",
            .send_text = send_text,
            .delete_text = delete_text,
            .send_userdata = NULL,
        };
        Steno *steno = steno_create(&steno_config);
        if (steno == NULL) {
            return 1;
        }
        const char *translation = NULL;
        if (steno_lookup_stroke(steno, argv[2], &translation)) {
            printf("%s -> %s\n", argv[2], translation);
        } else {
            printf("%s -> [untranslated]\n", argv[2]);
        }
        steno_destroy(steno);
        return 0;
    }

    if ((argc == 2 || argc == 3) && strcmp(argv[1], "--dump-dictionary") == 0) {
        const Steno_Config steno_config = {
            .keymap_path = NULL,
            .dictionary_path = "stoin-dictionary.json",
            .send_text = send_text,
            .delete_text = delete_text,
            .send_userdata = NULL,
        };
        Steno *steno = steno_create(&steno_config);
        if (steno == NULL) {
            return 1;
        }
        const char *path = argc == 3 ? argv[2] : "stoin-dictionary.json";
        if (!steno_dump_dictionary_json(steno, path)) {
            fprintf(stderr, "stoin: failed to dump dictionary to '%s'\n", path);
            steno_destroy(steno);
            return 1;
        }
        printf("stoin: wrote %zu entries to %s\n", steno_dictionary_count(steno), path);
        steno_destroy(steno);
        return 0;
    }

    Input_Mode input_mode = INPUT_MODE_GEMINI_PR;
    const char *gemini_port = NULL;
    int gemini_baud_rate = GEMINI_PR_DEFAULT_BAUD_RATE;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "qwerty") == 0) {
                input_mode = INPUT_MODE_QWERTY;
            } else if (strcmp(argv[i], "gemini-pr") == 0 || strcmp(argv[i], "gemini") == 0) {
                input_mode = INPUT_MODE_GEMINI_PR;
            } else {
                fprintf(stderr, "stoin: unknown input mode '%s'\n", argv[i]);
                print_usage();
                return 1;
            }
        } else if (strcmp(argv[i], "--gemini-port") == 0 && i + 1 < argc) {
            gemini_port = argv[++i];
        } else if (strcmp(argv[i], "--gemini-baud") == 0 && i + 1 < argc) {
            if (!parse_baud_rate(argv[++i], &gemini_baud_rate)) {
                fprintf(stderr, "stoin: invalid Gemini PR baud rate '%s'\n", argv[i]);
                return 1;
            }
        } else {
            print_usage();
            return 1;
        }
    }

    const Steno_Config steno_config = {
        .keymap_path = input_mode == INPUT_MODE_QWERTY ? "stoin.keymap" : NULL,
        .dictionary_path = "stoin-dictionary.json",
        .send_text = send_text,
        .delete_text = delete_text,
        .send_userdata = NULL,
    };

    App app = {
        .steno = steno_create(&steno_config),
    };
    if (app.steno == NULL) {
        return 1;
    }

    int status = 0;
    if (input_mode == INPUT_MODE_QWERTY) {
        status = run_qwerty(&app);
    } else {
        status = run_gemini_pr(&app, gemini_port, gemini_baud_rate);
    }

    steno_destroy(app.steno);
    return status;
}
