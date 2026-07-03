#include "gemini_pr.h"
#include "platform.h"
#include "raw_serial.h"
#include "runtime_config.h"
#include "stentura.h"
#include "steno.h"
#include "tx_bolt.h"
#include "tx_bolt_multiple.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef enum Input_Mode {
    INPUT_MODE_QWERTY,
    INPUT_MODE_TX_BOLT,
    INPUT_MODE_GEMINI_PR,
    INPUT_MODE_STENTURA,
} Input_Mode;

#define DEFAULT_DICTIONARY_PATH "lapwing-base.json"
#define DEFAULT_CONFIG_PATH "stoin-config.json"
#define DEFAULT_PEDAL_CONFIG_PATH "stoin-pedals.json"
#define DEFAULT_WORD_LIST_PATH "american_english_words.txt"

typedef struct App {
    Steno *steno;
    bool session_active;
    bool session_state_known;
    bool time_translations;
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

static bool send_key_combination(const char *combo, void *userdata)
{
    (void)userdata;
    return platform_send_key_combination(combo);
}

static bool update_session_active(App *app)
{
    if (app == NULL) {
        return false;
    }

    const bool active = platform_user_session_is_active();
    if (!app->session_state_known || app->session_active != active) {
        steno_set_session_active(app->steno, active);
        if (app->session_state_known) {
            fprintf(stderr,
                "stoin: user session %s; steno capture %s\n",
                active ? "active" : "inactive",
                active ? "resumed" : "suspended");
        } else if (!active) {
            fputs("stoin: user session inactive; steno capture suspended until login\n", stderr);
        }
        app->session_active = active;
        app->session_state_known = true;
    }

    return active;
}

static void run_app_maintenance(App *app)
{
    if (app == NULL) {
        return;
    }

    platform_pedals_poll();
    platform_file_watcher_poll();
    (void)update_session_active(app);
}

static void reload_dictionary_from_watcher(void *userdata)
{
    App *app = userdata;
    if (app != NULL) {
        (void)steno_reload_dictionary(app->steno);
    }
}

static void start_dictionary_watcher(App *app)
{
    if (app == NULL) {
        return;
    }

    const char *const *paths = NULL;
    size_t path_count = 0;
    if (!steno_get_dictionary_paths(app->steno, &paths, &path_count)
        || !platform_file_watcher_start(paths, path_count, reload_dictionary_from_watcher, app)) {
        fputs("stoin: warning: failed to start dictionary hot reload watcher\n", stderr);
    }
}

static void run_app_maintenance_callback(void *userdata)
{
    run_app_maintenance(userdata);
}

static void start_dictionary_watcher_callback(void *userdata)
{
    start_dictionary_watcher(userdata);
}

static bool app_session_active_callback(void *userdata)
{
    App *app = userdata;
    return app != NULL && app->session_state_known && app->session_active;
}

static bool handle_stroke_bits(App *app, uint64_t bits, uint64_t received_ns)
{
    if (app == NULL || app->steno == NULL) {
        return false;
    }

    if (app->time_translations) {
        platform_translation_timing_begin(received_ns);
    }
    platform_pedals_poll();
    const bool handled = steno_handle_stroke_bits(app->steno, bits);
    if (app->time_translations) {
        platform_translation_timing_cancel();
    }
    return handled;
}

static bool handle_stroke_bits_callback(uint64_t bits, uint64_t received_ns, void *userdata)
{
    App *app = userdata;
    return handle_stroke_bits(app, bits, received_ns);
}

static bool handle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;
    if (!update_session_active(app)) {
        return false;
    }

    if (app != NULL && app->time_translations && event != NULL && !event->is_down) {
        platform_translation_timing_begin(platform_monotonic_ns());
    }
    const bool handled = steno_handle_event(app->steno, event);
    if (app != NULL && app->time_translations) {
        platform_translation_timing_cancel();
    }
    return handled;
}

static void handle_pedal_event(Platform_Pedal_Role role, bool is_down, void *userdata)
{
    App *app = userdata;
    if (app == NULL || app->steno == NULL) {
        return;
    }

    switch (role) {
    case PLATFORM_PEDAL_ROLE_INITIAL_VERB:
        steno_set_phrase_namespace(app->steno, PHRASE_NAMESPACE_INITIAL_VERB, is_down);
        break;
    case PLATFORM_PEDAL_ROLE_FINAL_VERB:
        steno_set_phrase_namespace(app->steno, PHRASE_NAMESPACE_FINAL_VERB, is_down);
        break;
    case PLATFORM_PEDAL_ROLE_PHRASE_NONVERB:
        steno_set_phrase_namespace(app->steno, PHRASE_NAMESPACE_NONVERB, is_down);
        break;
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_COUNT:
    default:
        break;
    }
}

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

static void print_usage(void)
{
    fputs("usage: stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH]\n", stderr);
    fputs("             [--input tx-bolt|gemini-pr|stentura|qwerty]\n", stderr);
    fputs("             [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("             [--multiple-inputs] [--multi-input-window-ms MS]\n", stderr);
    fputs("             [--pedal-config PATH] [--register-pedal initial-verb|final-verb|nonverb]\n", stderr);
    fputs("             [--time-translations]\n", stderr);
    fputs("             [--trace-strokes|--no-trace-strokes]\n", stderr);
    fputs("       stoin --raw-serial [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] --lookup STROKE\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] --dump-dictionary [OUTPUT_PATH]\n", stderr);
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

static bool parse_milliseconds(const char *value, unsigned int *out_milliseconds)
{
    char *end = NULL;
    const unsigned long parsed = strtoul(value, &end, 10);
    if (value == end || *end != '\0' || parsed > 60000UL) {
        return false;
    }
    *out_milliseconds = (unsigned int)parsed;
    return true;
}

static bool parse_pedal_role(const char *value, Platform_Pedal_Role *out_role)
{
    if (value == NULL || out_role == NULL) {
        return false;
    }

    if (strcmp(value, "initial") == 0
        || strcmp(value, "initial-verb") == 0
        || strcmp(value, "initial_verb") == 0
        || strcmp(value, "iv") == 0
        || strcmp(value, "verb") == 0
        || strcmp(value, "core") == 0
        || strcmp(value, "phrase-core") == 0
        || strcmp(value, "phrase_core") == 0) {
        *out_role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
        return true;
    }
    if (strcmp(value, "nonverb") == 0
        || strcmp(value, "non-verb") == 0
        || strcmp(value, "phrase-nonverb") == 0
        || strcmp(value, "phrase_nonverb") == 0) {
        *out_role = PLATFORM_PEDAL_ROLE_PHRASE_NONVERB;
        return true;
    }
    if (strcmp(value, "final") == 0
        || strcmp(value, "final-verb") == 0
        || strcmp(value, "final_verb") == 0
        || strcmp(value, "fv") == 0) {
        *out_role = PLATFORM_PEDAL_ROLE_FINAL_VERB;
        return true;
    }
    return false;
}

static bool resolve_serial_port(const char *requested_port, char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0) {
        return false;
    }
    if (requested_port != NULL) {
        const int written = snprintf(out_path, out_size, "%s", requested_port);
        return written > 0 && (size_t)written < out_size;
    }
    return platform_find_serial_device(out_path, out_size);
}

static int run_qwerty(App *app)
{
    if (!platform_init(handle_input, app)) {
        return 1;
    }
    run_app_maintenance(app);
    start_dictionary_watcher(app);

#if defined(__linux__)
    puts("stoin: Linux evdev/uinput qwerty capture running");
#else
    puts("stoin: macOS qwerty event tap running");
#endif
    printf("stoin: loaded %zu key bindings and %zu dictionary entries\n",
        steno_key_binding_count(app->steno),
        steno_dictionary_count(app->steno));
    puts("stoin: steno capture starts enabled; press Ctrl+Esc to toggle it");
    puts("stoin: shortcut-modified keys pass through; press Ctrl+C in this terminal to quit");

    platform_run();
    platform_shutdown();
    return 0;
}

static int run_tx_bolt(App *app, const char *port_path, int baud_rate)
{
    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    const int resolved_baud_rate = baud_rate == 0 ? TX_BOLT_DEFAULT_BAUD_RATE : baud_rate;
    printf("stoin: TX Bolt serial capture starting at %d baud 8N1\n", resolved_baud_rate);
    printf("stoin: loaded %zu dictionary entries\n", steno_dictionary_count(app->steno));
    puts("stoin: press Ctrl+C in this terminal to quit");

    Tx_Bolt tx_bolt = {0};
    bool connected = false;
    bool output_ready = false;
    bool announced_disconnected = false;
    start_dictionary_watcher(app);

    while (!g_stop_requested) {
        run_app_maintenance(app);
        const bool session_active = app->session_state_known && app->session_active;

        if (!connected) {
            char resolved_port_path[256] = {0};
            if (!resolve_serial_port(port_path, resolved_port_path, sizeof(resolved_port_path))) {
                if (!announced_disconnected) {
                    if (port_path != NULL) {
                        fprintf(stderr, "stoin: TX Bolt disconnected; waiting for %s\n", port_path);
                    } else {
                        fputs("stoin: TX Bolt disconnected; waiting for a platform default serial device\n", stderr);
                    }
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            const Tx_Bolt_Config tx_bolt_config = {
                .port_path = resolved_port_path,
                .baud_rate = resolved_baud_rate,
            };
            errno = 0;
            if (!tx_bolt_open(&tx_bolt, &tx_bolt_config)) {
                if (!announced_disconnected) {
                    fprintf(stderr, "stoin: TX Bolt disconnected; waiting for %s", resolved_port_path);
                    if (errno != 0) {
                        fprintf(stderr, " (%s)", strerror(errno));
                    }
                    fputc('\n', stderr);
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            if (!output_ready && !platform_output_init()) {
                tx_bolt_close(&tx_bolt);
                platform_shutdown();
                return 1;
            }
            output_ready = true;
            connected = true;
            announced_disconnected = false;
            printf("stoin: TX Bolt connected on %s\n", tx_bolt_port_path(&tx_bolt));
        }

        uint64_t stroke_bits = 0;
        if (tx_bolt_read_stroke(&tx_bolt, &stroke_bits)) {
            if (session_active) {
                (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
            }
        } else if (tx_bolt_had_error(&tx_bolt)) {
            printf("stoin: TX Bolt disconnected from %s; waiting for reconnect\n", tx_bolt_port_path(&tx_bolt));
            tx_bolt_close(&tx_bolt);
            connected = false;
            platform_sleep_ms(1000);
        }
    }

    if (connected) {
        tx_bolt_close(&tx_bolt);
    }
    platform_shutdown();
    return 0;
}

static int run_gemini_pr(App *app, const char *port_path, int baud_rate)
{
    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    const int resolved_baud_rate = baud_rate == 0 ? GEMINI_PR_DEFAULT_BAUD_RATE : baud_rate;
    printf("stoin: Gemini PR serial capture starting at %d baud 8N1\n", resolved_baud_rate);
    printf("stoin: loaded %zu dictionary entries\n", steno_dictionary_count(app->steno));
    puts("stoin: press Ctrl+C in this terminal to quit");

    Gemini_Pr gemini = {0};
    bool connected = false;
    bool output_ready = false;
    bool announced_disconnected = false;
    start_dictionary_watcher(app);

    while (!g_stop_requested) {
        run_app_maintenance(app);
        const bool session_active = app->session_state_known && app->session_active;

        if (!connected) {
            char resolved_port_path[256] = {0};
            if (!resolve_serial_port(port_path, resolved_port_path, sizeof(resolved_port_path))) {
                if (!announced_disconnected) {
                    if (port_path != NULL) {
                        fprintf(stderr, "stoin: Gemini PR disconnected; waiting for %s\n", port_path);
                    } else {
                        fputs("stoin: Gemini PR disconnected; waiting for a platform default serial device\n", stderr);
                    }
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            const Gemini_Pr_Config gemini_config = {
                .port_path = resolved_port_path,
                .baud_rate = resolved_baud_rate,
            };
            errno = 0;
            if (!gemini_pr_open(&gemini, &gemini_config)) {
                if (!announced_disconnected) {
                    fprintf(stderr, "stoin: Gemini PR disconnected; waiting for %s", resolved_port_path);
                    if (errno != 0) {
                        fprintf(stderr, " (%s)", strerror(errno));
                    }
                    fputc('\n', stderr);
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            if (!output_ready && !platform_output_init()) {
                gemini_pr_close(&gemini);
                platform_shutdown();
                return 1;
            }
            output_ready = true;
            connected = true;
            announced_disconnected = false;
            printf("stoin: Gemini PR connected on %s\n", gemini_pr_port_path(&gemini));
        }

        uint64_t stroke_bits = 0;
        if (gemini_pr_read_stroke(&gemini, &stroke_bits)) {
            if (session_active) {
                (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
            }
        } else if (gemini_pr_had_error(&gemini)) {
            printf("stoin: Gemini PR disconnected from %s; waiting for reconnect\n", gemini_pr_port_path(&gemini));
            gemini_pr_close(&gemini);
            connected = false;
            platform_sleep_ms(1000);
        }
    }

    if (connected) {
        gemini_pr_close(&gemini);
    }
    platform_shutdown();
    return 0;
}

static int run_stentura(App *app, const char *port_path, int baud_rate)
{
    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    const int resolved_baud_rate = baud_rate == 0 ? STENTURA_DEFAULT_BAUD_RATE : baud_rate;
    printf("stoin: Stentura serial capture starting at %d baud 8N1\n", resolved_baud_rate);
    printf("stoin: loaded %zu dictionary entries\n", steno_dictionary_count(app->steno));
    puts("stoin: press Ctrl+C in this terminal to quit");

    Stentura stentura = {0};
    bool connected = false;
    bool output_ready = false;
    bool announced_disconnected = false;
    start_dictionary_watcher(app);

    while (!g_stop_requested) {
        run_app_maintenance(app);
        const bool session_active = app->session_state_known && app->session_active;

        if (!connected) {
            bool opened = false;
            char resolved_port_path[PLATFORM_SERIAL_PATH_MAX] = {0};
            if (port_path != NULL) {
                const int written = snprintf(resolved_port_path, sizeof(resolved_port_path), "%s", port_path);
                if (written > 0 && (size_t)written < sizeof(resolved_port_path)) {
                    const Stentura_Config stentura_config = {
                        .port_path = resolved_port_path,
                        .baud_rate = resolved_baud_rate,
                    };
                    errno = 0;
                    opened = stentura_open(&stentura, &stentura_config);
                } else {
                    errno = ENAMETOOLONG;
                }
            } else {
                char serial_paths[16][PLATFORM_SERIAL_PATH_MAX] = {{0}};
                const size_t serial_path_count =
                    platform_find_serial_devices(serial_paths, sizeof(serial_paths) / sizeof(serial_paths[0]));
                for (size_t i = 0; i < serial_path_count && !opened; ++i) {
                    const Stentura_Config stentura_config = {
                        .port_path = serial_paths[i],
                        .baud_rate = resolved_baud_rate,
                    };
                    errno = 0;
                    if (stentura_open(&stentura, &stentura_config)) {
                        const int written =
                            snprintf(resolved_port_path, sizeof(resolved_port_path), "%s", serial_paths[i]);
                        opened = written > 0 && (size_t)written < sizeof(resolved_port_path);
                        if (!opened) {
                            stentura_close(&stentura);
                            errno = ENAMETOOLONG;
                        }
                    }
                }
            }

            if (!opened) {
                if (!announced_disconnected) {
                    if (port_path != NULL) {
                        fprintf(stderr, "stoin: Stentura disconnected; waiting for %s", port_path);
                    } else {
                        fputs("stoin: Stentura disconnected; waiting for a Stentura-compatible serial device", stderr);
                    }
                    if (port_path != NULL && errno != 0) {
                        fprintf(stderr, " (%s)", strerror(errno));
                    }
                    fputc('\n', stderr);
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            if (!output_ready && !platform_output_init()) {
                stentura_close(&stentura);
                platform_shutdown();
                return 1;
            }
            output_ready = true;
            connected = true;
            announced_disconnected = false;
            printf("stoin: Stentura connected on %s\n", stentura_port_path(&stentura));
        }

        uint64_t stroke_bits = 0;
        if (stentura_read_stroke(&stentura, &stroke_bits)) {
            if (session_active) {
                (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
            }
        } else if (stentura_had_error(&stentura)) {
            printf("stoin: Stentura disconnected from %s; waiting for reconnect\n", stentura_port_path(&stentura));
            stentura_close(&stentura);
            connected = false;
            platform_sleep_ms(1000);
        }
    }

    if (connected) {
        stentura_close(&stentura);
    }
    platform_shutdown();
    return 0;
}

static Steno *create_steno(
    char *const *dictionary_paths,
    const bool *dictionary_enabled,
    size_t dictionary_path_count,
    const char *word_list_path,
    const char *keymap_path,
    FILE *trace_file
)
{
    const Steno_Config steno_config = {
        .keymap_path = keymap_path,
        .dictionary_paths = (const char *const *)dictionary_paths,
        .dictionary_enabled = dictionary_enabled,
        .dictionary_path_count = dictionary_path_count,
        .word_list_path = word_list_path,
        .send_text = send_text,
        .delete_text = delete_text,
        .send_key_combination = send_key_combination,
        .send_userdata = NULL,
        .trace_file = trace_file,
    };
    return steno_create(&steno_config);
}

int main(int argc, char **argv)
{
    const char *config_path = DEFAULT_CONFIG_PATH;
    bool config_path_explicit = false;
    const char *lookup_stroke = NULL;
    bool dump_dictionary = false;
    bool raw_serial_dump = false;
    const char *dump_path = NULL;
    Input_Mode input_mode = INPUT_MODE_TX_BOLT;
    const char *serial_port = NULL;
    int serial_baud_rate = TX_BOLT_DEFAULT_BAUD_RATE;
    bool multiple_inputs = false;
    unsigned int multi_input_window_ms = TX_BOLT_MULTIPLE_DEFAULT_WINDOW_MS;
    bool trace_strokes = true;
    bool time_translations = false;
    const char *pedal_config_path = DEFAULT_PEDAL_CONFIG_PATH;
    Platform_Pedal_Role register_pedal_role = PLATFORM_PEDAL_ROLE_NONE;

    bool raw_serial_requested = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
            config_path_explicit = true;
        } else if (strcmp(argv[i], "--raw-serial") == 0 || strcmp(argv[i], "--dump-serial") == 0) {
            raw_serial_requested = true;
        }
    }

    Runtime_Config runtime_config = {0};
    if (!raw_serial_requested
        && (!runtime_config_set_word_list(&runtime_config, DEFAULT_WORD_LIST_PATH)
            || !runtime_config_load(&runtime_config, config_path, !config_path_explicit))) {
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    bool cli_dictionary_paths = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            ++i;
        } else if (strcmp(argv[i], "--dictionary") == 0 && i + 1 < argc) {
            if (!cli_dictionary_paths) {
                runtime_config_clear_dictionaries(&runtime_config);
                cli_dictionary_paths = true;
            }
            if (!runtime_config_add_dictionary(&runtime_config, argv[++i])) {
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if (strcmp(argv[i], "--word-list") == 0 && i + 1 < argc) {
            if (!runtime_config_set_word_list(&runtime_config, argv[++i])) {
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if (strcmp(argv[i], "--lookup") == 0 && i + 1 < argc) {
            if (lookup_stroke != NULL || dump_dictionary || raw_serial_dump) {
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            lookup_stroke = argv[++i];
        } else if (strcmp(argv[i], "--dump-dictionary") == 0) {
            if (lookup_stroke != NULL || dump_dictionary || raw_serial_dump) {
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            dump_dictionary = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dump_path = argv[++i];
            }
        } else if (strcmp(argv[i], "--raw-serial") == 0 || strcmp(argv[i], "--dump-serial") == 0) {
            if (lookup_stroke != NULL || dump_dictionary || raw_serial_dump) {
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            raw_serial_dump = true;
        } else if (strcmp(argv[i], "--trace-strokes") == 0) {
            trace_strokes = true;
        } else if (strcmp(argv[i], "--no-trace-strokes") == 0) {
            trace_strokes = false;
        } else if (strcmp(argv[i], "--time-translations") == 0
            || strcmp(argv[i], "--time-translation") == 0) {
            time_translations = true;
        } else if (strcmp(argv[i], "--multiple-inputs") == 0) {
            multiple_inputs = true;
        } else if (strcmp(argv[i], "--multi-input-window-ms") == 0 && i + 1 < argc) {
            if (!parse_milliseconds(argv[++i], &multi_input_window_ms)) {
                fprintf(stderr, "stoin: invalid multi-input window '%s'\n", argv[i]);
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if (strcmp(argv[i], "--pedal-config") == 0 && i + 1 < argc) {
            pedal_config_path = argv[++i];
        } else if (strcmp(argv[i], "--register-pedals") == 0) {
            register_pedal_role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
        } else if (strcmp(argv[i], "--register-pedal") == 0 && i + 1 < argc) {
            if (!parse_pedal_role(argv[++i], &register_pedal_role)) {
                fprintf(stderr, "stoin: unknown pedal role '%s'\n", argv[i]);
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "qwerty") == 0) {
                input_mode = INPUT_MODE_QWERTY;
            } else if (strcmp(argv[i], "tx-bolt") == 0
                || strcmp(argv[i], "txbolt") == 0
                || strcmp(argv[i], "bolt") == 0) {
                input_mode = INPUT_MODE_TX_BOLT;
            } else if (strcmp(argv[i], "gemini-pr") == 0 || strcmp(argv[i], "gemini") == 0) {
                input_mode = INPUT_MODE_GEMINI_PR;
            } else if (strcmp(argv[i], "stentura") == 0
                || strcmp(argv[i], "stenograph") == 0
                || strcmp(argv[i], "stenograph-8000") == 0
                || strcmp(argv[i], "8000") == 0) {
                input_mode = INPUT_MODE_STENTURA;
            } else {
                fprintf(stderr, "stoin: unknown input mode '%s'\n", argv[i]);
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if ((strcmp(argv[i], "--serial-port") == 0
                || strcmp(argv[i], "--port") == 0
                || strcmp(argv[i], "--tx-bolt-port") == 0
                || strcmp(argv[i], "--gemini-port") == 0
                || strcmp(argv[i], "--stentura-port") == 0) && i + 1 < argc) {
            serial_port = argv[++i];
        } else if ((strcmp(argv[i], "--serial-baud") == 0
                || strcmp(argv[i], "--baud") == 0
                || strcmp(argv[i], "--tx-bolt-baud") == 0
                || strcmp(argv[i], "--gemini-baud") == 0
                || strcmp(argv[i], "--stentura-baud") == 0) && i + 1 < argc) {
            if (!parse_baud_rate(argv[++i], &serial_baud_rate)) {
                fprintf(stderr, "stoin: invalid serial baud rate '%s'\n", argv[i]);
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else {
            print_usage();
            runtime_config_destroy(&runtime_config);
            return 1;
        }
    }

    if (multiple_inputs && input_mode != INPUT_MODE_TX_BOLT) {
        fputs("stoin: --multiple-inputs currently only supports --input tx-bolt\n", stderr);
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    if (raw_serial_dump) {
        const int status = raw_serial_run(serial_port, serial_baud_rate);
        runtime_config_destroy(&runtime_config);
        return status;
    }

    if (arrlenu(runtime_config.dictionary_paths) == 0
        && !runtime_config_add_dictionary(&runtime_config, DEFAULT_DICTIONARY_PATH)) {
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    if (lookup_stroke != NULL) {
        Steno *steno = create_steno(
            runtime_config.dictionary_paths,
            runtime_config.dictionary_enabled,
            arrlenu(runtime_config.dictionary_paths),
            runtime_config.word_list_path,
            NULL,
            NULL
        );
        if (steno == NULL) {
            runtime_config_destroy(&runtime_config);
            return 1;
        }
        const char *translation = NULL;
        if (steno_lookup_stroke(steno, lookup_stroke, &translation)) {
            printf("%s -> %s\n", lookup_stroke, translation);
        } else {
            printf("%s -> [untranslated]\n", lookup_stroke);
        }
        steno_destroy(steno);
        runtime_config_destroy(&runtime_config);
        return 0;
    }

    if (dump_dictionary) {
        Steno *steno = create_steno(
            runtime_config.dictionary_paths,
            runtime_config.dictionary_enabled,
            arrlenu(runtime_config.dictionary_paths),
            runtime_config.word_list_path,
            NULL,
            NULL
        );
        if (steno == NULL) {
            runtime_config_destroy(&runtime_config);
            return 1;
        }
        const char *path = dump_path != NULL ? dump_path : runtime_config.dictionary_paths[arrlenu(runtime_config.dictionary_paths) - 1];
        if (!steno_dump_dictionary_json(steno, path)) {
            fprintf(stderr, "stoin: failed to dump dictionary to '%s'\n", path);
            steno_destroy(steno);
            runtime_config_destroy(&runtime_config);
            return 1;
        }
        printf("stoin: wrote %zu entries to %s\n", steno_dictionary_count(steno), path);
        steno_destroy(steno);
        runtime_config_destroy(&runtime_config);
        return 0;
    }

    App app = {
        .steno = create_steno(
            runtime_config.dictionary_paths,
            runtime_config.dictionary_enabled,
            arrlenu(runtime_config.dictionary_paths),
            runtime_config.word_list_path,
            input_mode == INPUT_MODE_QWERTY ? "stoin.keymap" : NULL,
            trace_strokes ? stderr : NULL
        ),
        .time_translations = time_translations,
    };
    if (app.steno == NULL) {
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    platform_translation_timing_set_enabled(time_translations);
    if (time_translations) {
        fputs("stoin: translation timing enabled; latency stops immediately before the first platform output event\n", stderr);
        if (trace_strokes) {
            fputs("stoin: note: stroke tracing is enabled and included in the measured path; use --no-trace-strokes for cleaner benchmark numbers\n", stderr);
        }
    }

    if (!platform_pedals_init(pedal_config_path, register_pedal_role, handle_pedal_event, &app)) {
        steno_destroy(app.steno);
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    int status = 0;
    if (input_mode == INPUT_MODE_QWERTY) {
        status = run_qwerty(&app);
    } else if (input_mode == INPUT_MODE_TX_BOLT) {
        if (multiple_inputs) {
            const Tx_Bolt_Multiple_Config tx_bolt_multiple_config = {
                .port_path = serial_port,
                .baud_rate = serial_baud_rate,
                .merge_window_ms = multi_input_window_ms,
                .dictionary_count = steno_dictionary_count(app.steno),
                .start_watcher = start_dictionary_watcher_callback,
                .run_maintenance = run_app_maintenance_callback,
                .session_active = app_session_active_callback,
                .handle_stroke = handle_stroke_bits_callback,
                .userdata = &app,
            };
            status = tx_bolt_multiple_run(&tx_bolt_multiple_config);
        } else {
            status = run_tx_bolt(&app, serial_port, serial_baud_rate);
        }
    } else if (input_mode == INPUT_MODE_GEMINI_PR) {
        status = run_gemini_pr(&app, serial_port, serial_baud_rate);
    } else {
        status = run_stentura(&app, serial_port, serial_baud_rate);
    }

    steno_destroy(app.steno);
    runtime_config_destroy(&runtime_config);
    return status;
}
