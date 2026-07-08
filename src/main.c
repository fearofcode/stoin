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
#define DEFAULT_WORD_LIST_PATH "american_english_words.txt"
#define DEFAULT_PHRASING_PATH "phrasing.json"
#define INPUT_EVENT_POLL_SLEEP_MS 10
#define TX_BOLT_STROKE_IDLE_FLUSH_MS 100

typedef struct App {
    Steno *steno;
    bool session_active;
    bool session_state_known;
    bool time_translations;
    bool trace_key_events;
    bool qwerty_input;
    bool phrase_toggle_enabled;
    Platform_Atomic_Bool *phrase_toggle_down;
    Platform_Atomic_Bool *phrase_stroke_latched;
    uint16_t phrase_toggle_keycode;
    const char *phrase_toggle_name;
    bool nonverb_phrase_toggle_enabled;
    Platform_Atomic_Bool *nonverb_phrase_toggle_down;
    Platform_Atomic_Bool *nonverb_phrase_stroke_latched;
    uint16_t nonverb_phrase_toggle_keycode;
    const char *nonverb_phrase_toggle_name;
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

    platform_file_watcher_poll();
    (void)update_session_active(app);
}

static void reload_watched_files(void *userdata)
{
    App *app = userdata;
    if (app != NULL) {
        (void)steno_reload_dictionary_if_changed(app->steno);
        (void)steno_reload_phrasing_if_changed(app->steno);
    }
}

static void start_dictionary_watcher(App *app)
{
    if (app == NULL) {
        return;
    }

    const char *const *paths = NULL;
    size_t path_count = 0;
    const char **watch_paths = NULL;
    if (!steno_get_dictionary_paths(app->steno, &paths, &path_count)) {
        fputs("stoin: warning: failed to start hot reload watcher\n", stderr);
        return;
    }
    for (size_t i = 0; i < path_count; ++i) {
        arrput(watch_paths, paths[i]);
    }
    const char *phrasing_path = NULL;
    if (steno_get_phrasing_path(app->steno, &phrasing_path) && phrasing_path != NULL) {
        arrput(watch_paths, phrasing_path);
    }

    const bool started = arrlenu(watch_paths) > 0
        && platform_file_watcher_start(
            (const char *const *)watch_paths,
            arrlenu(watch_paths),
            reload_watched_files,
            app
        );
    arrfree(watch_paths);
    if (!started) {
        fputs("stoin: warning: failed to start hot reload watcher\n", stderr);
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

static bool app_phrase_namespace_enabled(const App *app)
{
    return app != NULL && (app->phrase_toggle_enabled || app->nonverb_phrase_toggle_enabled);
}

static bool app_toggle_active(
    Platform_Atomic_Bool *down,
    Platform_Atomic_Bool *latched,
    bool consume_latch
)
{
    const bool is_down = platform_atomic_bool_load(down);
    const bool was_latched = consume_latch
        ? platform_atomic_bool_exchange(latched, false)
        : platform_atomic_bool_load(latched);
    return is_down || was_latched;
}

static Steno_Phrase_Mode app_phrase_mode_from_active(
    const App *app,
    bool phrase_active,
    bool nonverb_active
)
{
    if (phrase_active && nonverb_active) {
        return STENO_PHRASE_MODE_ALL;
    }
    if (phrase_active) {
        return app != NULL && app->nonverb_phrase_toggle_enabled
            ? STENO_PHRASE_MODE_VERBS
            : STENO_PHRASE_MODE_ALL;
    }
    if (nonverb_active) {
        return STENO_PHRASE_MODE_NONVERBS;
    }
    return STENO_PHRASE_MODE_NONE;
}

static Steno_Phrase_Mode app_current_phrase_mode(App *app, bool consume_latches)
{
    if (app == NULL) {
        return STENO_PHRASE_MODE_NONE;
    }

    const bool phrase_active = app->phrase_toggle_enabled
        && app_toggle_active(app->phrase_toggle_down, app->phrase_stroke_latched, consume_latches);
    const bool nonverb_active = app->nonverb_phrase_toggle_enabled
        && app_toggle_active(
            app->nonverb_phrase_toggle_down,
            app->nonverb_phrase_stroke_latched,
            consume_latches
        );
    return app_phrase_mode_from_active(app, phrase_active, nonverb_active);
}

static Steno_Phrase_Mode app_current_phrase_down_mode(const App *app)
{
    if (app == NULL) {
        return STENO_PHRASE_MODE_NONE;
    }

    const bool phrase_active = app->phrase_toggle_enabled
        && platform_atomic_bool_load(app->phrase_toggle_down);
    const bool nonverb_active = app->nonverb_phrase_toggle_enabled
        && platform_atomic_bool_load(app->nonverb_phrase_toggle_down);
    return app_phrase_mode_from_active(app, phrase_active, nonverb_active);
}

static bool handle_stroke_bits(App *app, uint64_t bits, uint64_t received_ns)
{
    if (app == NULL || app->steno == NULL) {
        return false;
    }

    if (app->time_translations) {
        platform_translation_timing_begin(received_ns);
    }
    Stroke_Input stroke = {
        .bits = bits,
        .received_ns = received_ns,
        .phrase_mode = app_current_phrase_mode(app, true),
        .phrase_namespace = app_phrase_namespace_enabled(app),
    };
    const bool handled = steno_handle_stroke(app->steno, stroke);
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

static void print_key_event(const Input_Event *event)
{
    if (event == NULL) {
        return;
    }

    printf("stoin: key event keycode=%u %s",
        (unsigned int)event->keycode,
        event->is_down ? "down" : "up");
    if (event->printable != '\0') {
        printf(" printable='%c'", event->printable);
    }
    printf(" shift=%u control=%u option=%u command=%u",
        event->shift ? 1u : 0u,
        event->control ? 1u : 0u,
        event->option ? 1u : 0u,
        event->command ? 1u : 0u);
    putchar('\n');
    fflush(stdout);
}

static void update_phrase_toggle_state(
    Platform_Atomic_Bool *down,
    Platform_Atomic_Bool *latched,
    const Input_Event *event
)
{
    if (!event->is_repeat) {
        platform_atomic_bool_store(down, event->is_down);
        if (event->is_down) {
            platform_atomic_bool_store(latched, true);
        }
    }
}

static bool update_phrase_toggle_from_event(App *app, const Input_Event *event, bool update_steno)
{
    if (app == NULL || event == NULL) {
        return false;
    }

    if (app->phrase_toggle_enabled && event->keycode == app->phrase_toggle_keycode) {
        update_phrase_toggle_state(app->phrase_toggle_down, app->phrase_stroke_latched, event);
        if (update_steno) {
            steno_set_phrase_mode_family(app->steno, app_current_phrase_down_mode(app));
        }
        return true;
    }

    if (app->nonverb_phrase_toggle_enabled && event->keycode == app->nonverb_phrase_toggle_keycode) {
        update_phrase_toggle_state(
            app->nonverb_phrase_toggle_down,
            app->nonverb_phrase_stroke_latched,
            event
        );
        if (update_steno) {
            steno_set_phrase_mode_family(app->steno, app_current_phrase_down_mode(app));
        }
        return true;
    }

    return false;
}

static bool handle_phrase_toggle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;

    if (app != NULL && event != NULL && app->trace_key_events && !event->is_repeat) {
        print_key_event(event);
    }

    return update_phrase_toggle_from_event(app, event, false);
}

static bool handle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;
    if (!update_session_active(app)) {
        return false;
    }

    if (app != NULL && event != NULL && app->trace_key_events && !event->is_repeat) {
        print_key_event(event);
    }

    if (update_phrase_toggle_from_event(app, event, true)) {
        return true;
    }

    if (app == NULL || !app->qwerty_input) {
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

static bool app_wants_keyboard_events(const App *app)
{
    return app != NULL
        && (app->phrase_toggle_enabled || app->nonverb_phrase_toggle_enabled || app->trace_key_events);
}

static void app_destroy_phrase_toggle_state(App *app)
{
    if (app == NULL) {
        return;
    }
    platform_atomic_bool_destroy(app->phrase_toggle_down);
    platform_atomic_bool_destroy(app->phrase_stroke_latched);
    platform_atomic_bool_destroy(app->nonverb_phrase_toggle_down);
    platform_atomic_bool_destroy(app->nonverb_phrase_stroke_latched);
    app->phrase_toggle_down = NULL;
    app->phrase_stroke_latched = NULL;
    app->nonverb_phrase_toggle_down = NULL;
    app->nonverb_phrase_stroke_latched = NULL;
}

static bool app_init_phrase_toggle_state(App *app)
{
    if (app == NULL) {
        return false;
    }
    app->phrase_toggle_down = platform_atomic_bool_create(false);
    app->phrase_stroke_latched = platform_atomic_bool_create(false);
    app->nonverb_phrase_toggle_down = platform_atomic_bool_create(false);
    app->nonverb_phrase_stroke_latched = platform_atomic_bool_create(false);
    if (app->phrase_toggle_down == NULL
        || app->phrase_stroke_latched == NULL
        || app->nonverb_phrase_toggle_down == NULL
        || app->nonverb_phrase_stroke_latched == NULL) {
        app_destroy_phrase_toggle_state(app);
        return false;
    }
    return true;
}

static void print_phrase_toggle_status(const App *app)
{
    if (app != NULL && app->phrase_toggle_enabled) {
        printf("stoin: %sphrase toggle %s enabled (keycode %u)\n",
            app->nonverb_phrase_toggle_enabled ? "verb " : "",
            app->phrase_toggle_name,
            (unsigned int)app->phrase_toggle_keycode);
    }
    if (app != NULL && app->nonverb_phrase_toggle_enabled) {
        printf("stoin: nonverb phrase toggle %s enabled (keycode %u)\n",
            app->nonverb_phrase_toggle_name,
            (unsigned int)app->nonverb_phrase_toggle_keycode);
    }
}

static void sleep_with_input_events(const App *app, unsigned int milliseconds)
{
    if (!app_wants_keyboard_events(app)) {
        platform_sleep_ms(milliseconds);
        return;
    }

    const uint64_t start_ms = platform_monotonic_ms();
    while (platform_monotonic_ms() - start_ms < milliseconds) {
        platform_poll_input_events();
        platform_sleep_ms(INPUT_EVENT_POLL_SLEEP_MS);
    }
    platform_poll_input_events();
}

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

static void print_usage(void)
{
    fputs("usage: stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] [--phrasing PATH]\n", stderr);
    fputs("             [--input tx-bolt|gemini-pr|stentura|qwerty]\n", stderr);
    fputs("             [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("             [--multiple-inputs] [--multi-input-window-ms MS]\n", stderr);
    fputs("             [--phrase-toggle KEY] [--nonverb-phrase-toggle KEY]\n", stderr);
    fputs("             [--trace-key-events]\n", stderr);
    fputs("             [--print-suggestions] [--suggestion-log PATH]\n", stderr);
    fputs("             [--time-translations]\n", stderr);
    fputs("             [--trace-strokes|--no-trace-strokes]\n", stderr);
    fputs("       stoin --raw-serial [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] [--phrasing PATH] --lookup STROKE\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] [--phrasing PATH] --dump-dictionary [OUTPUT_PATH]\n", stderr);
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
    print_phrase_toggle_status(app);
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
    uint64_t last_byte_ms = 0;
    start_dictionary_watcher(app);

    while (!g_stop_requested) {
        run_app_maintenance(app);
        platform_poll_input_events();
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
                sleep_with_input_events(app, 1000);
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
                sleep_with_input_events(app, 1000);
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
            last_byte_ms = 0;
            printf("stoin: TX Bolt connected on %s\n", tx_bolt_port_path(&tx_bolt));
        }

        bool made_progress = false;
        while (true) {
            uint64_t stroke_bits = 0;
            bool read_byte = false;
            if (tx_bolt_read_stroke_nonblocking(&tx_bolt, &stroke_bits, &read_byte)) {
                platform_poll_input_events();
                if (read_byte) {
                    last_byte_ms = platform_monotonic_ms();
                }
                if (session_active) {
                    (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
                }
                made_progress = true;
                continue;
            }
            if (read_byte) {
                last_byte_ms = platform_monotonic_ms();
                made_progress = true;
                continue;
            }
            break;
        }

        if (!tx_bolt_had_error(&tx_bolt)
            && tx_bolt_has_partial_stroke(&tx_bolt)
            && last_byte_ms != 0) {
            const uint64_t now_ms = platform_monotonic_ms();
            if (now_ms - last_byte_ms >= TX_BOLT_STROKE_IDLE_FLUSH_MS) {
                uint64_t stroke_bits = 0;
                last_byte_ms = 0;
                if (tx_bolt_flush_stroke(&tx_bolt, &stroke_bits)) {
                    platform_poll_input_events();
                    if (session_active) {
                        (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
                    }
                    made_progress = true;
                }
            }
        }

        if (tx_bolt_had_error(&tx_bolt)) {
            printf("stoin: TX Bolt disconnected from %s; waiting for reconnect\n", tx_bolt_port_path(&tx_bolt));
            tx_bolt_close(&tx_bolt);
            connected = false;
            last_byte_ms = 0;
            sleep_with_input_events(app, 1000);
        } else if (!made_progress) {
            sleep_with_input_events(app, INPUT_EVENT_POLL_SLEEP_MS);
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
        platform_poll_input_events();
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
                sleep_with_input_events(app, 1000);
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
                sleep_with_input_events(app, 1000);
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
            platform_poll_input_events();
            if (session_active) {
                (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
            }
        } else if (gemini_pr_had_error(&gemini)) {
            printf("stoin: Gemini PR disconnected from %s; waiting for reconnect\n", gemini_pr_port_path(&gemini));
            gemini_pr_close(&gemini);
            connected = false;
            sleep_with_input_events(app, 1000);
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
        platform_poll_input_events();
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
                sleep_with_input_events(app, 1000);
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
            platform_poll_input_events();
            if (session_active) {
                (void)handle_stroke_bits(app, stroke_bits, platform_monotonic_ns());
            }
        } else if (stentura_had_error(&stentura)) {
            printf("stoin: Stentura disconnected from %s; waiting for reconnect\n", stentura_port_path(&stentura));
            stentura_close(&stentura);
            connected = false;
            sleep_with_input_events(app, 1000);
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
    const char *phrasing_path,
    const char *keymap_path,
    bool print_suggestions,
    FILE *suggestion_log_file,
    FILE *trace_file
)
{
    const Steno_Config steno_config = {
        .keymap_path = keymap_path,
        .dictionary_paths = (const char *const *)dictionary_paths,
        .dictionary_enabled = dictionary_enabled,
        .dictionary_path_count = dictionary_path_count,
        .word_list_path = word_list_path,
        .phrasing_path = phrasing_path,
        .send_text = send_text,
        .delete_text = delete_text,
        .send_key_combination = send_key_combination,
        .send_userdata = NULL,
        .trace_file = trace_file,
        .suggestion_log_file = suggestion_log_file,
        .print_suggestions = print_suggestions,
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
    bool trace_key_events = false;
    bool print_suggestions = false;
    const char *suggestion_log_path = NULL;
    bool time_translations = false;
    bool phrase_toggle_enabled = false;
    uint16_t phrase_toggle_keycode = 0;
    const char *phrase_toggle_name = NULL;
    bool nonverb_phrase_toggle_enabled = false;
    uint16_t nonverb_phrase_toggle_keycode = 0;
    const char *nonverb_phrase_toggle_name = NULL;

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
            || !runtime_config_set_phrasing(&runtime_config, DEFAULT_PHRASING_PATH)
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
        } else if (strcmp(argv[i], "--phrasing") == 0 && i + 1 < argc) {
            if (!runtime_config_set_phrasing(&runtime_config, argv[++i])) {
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
        } else if (strcmp(argv[i], "--trace-key-events") == 0
            || strcmp(argv[i], "--trace-input-events") == 0) {
            trace_key_events = true;
        } else if (strcmp(argv[i], "--print-suggestions") == 0) {
            print_suggestions = true;
        } else if (strcmp(argv[i], "--suggestion-log") == 0 && i + 1 < argc) {
            suggestion_log_path = argv[++i];
        } else if (strcmp(argv[i], "--time-translations") == 0
            || strcmp(argv[i], "--time-translation") == 0) {
            time_translations = true;
        } else if ((strcmp(argv[i], "--phrase-toggle") == 0
                || strcmp(argv[i], "--phase-toggle") == 0) && i + 1 < argc) {
            phrase_toggle_name = argv[++i];
            if (!platform_keycode_from_name(phrase_toggle_name, &phrase_toggle_keycode)) {
                fprintf(stderr, "stoin: unknown phrase toggle key '%s'\n", phrase_toggle_name);
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            phrase_toggle_enabled = true;
        } else if ((strcmp(argv[i], "--nonverb-phrase-toggle") == 0
                || strcmp(argv[i], "--nonverb-phase-toggle") == 0
                || strcmp(argv[i], "--nonverb-toggle") == 0) && i + 1 < argc) {
            nonverb_phrase_toggle_name = argv[++i];
            if (!platform_keycode_from_name(nonverb_phrase_toggle_name, &nonverb_phrase_toggle_keycode)) {
                fprintf(stderr, "stoin: unknown nonverb phrase toggle key '%s'\n", nonverb_phrase_toggle_name);
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            nonverb_phrase_toggle_enabled = true;
        } else if (strcmp(argv[i], "--multiple-inputs") == 0) {
            multiple_inputs = true;
        } else if (strcmp(argv[i], "--multi-input-window-ms") == 0 && i + 1 < argc) {
            if (!parse_milliseconds(argv[++i], &multi_input_window_ms)) {
                fprintf(stderr, "stoin: invalid multi-input window '%s'\n", argv[i]);
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
    if (phrase_toggle_enabled
        && nonverb_phrase_toggle_enabled
        && phrase_toggle_keycode == nonverb_phrase_toggle_keycode) {
        fprintf(stderr,
            "stoin: --phrase-toggle and --nonverb-phrase-toggle must use distinct keys; both resolved to keycode %u\n",
            (unsigned int)phrase_toggle_keycode);
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
            runtime_config.phrasing_path,
            NULL,
            false,
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
            runtime_config.phrasing_path,
            NULL,
            false,
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

    FILE *suggestion_log_file = NULL;
    if (suggestion_log_path != NULL) {
        suggestion_log_file = fopen(suggestion_log_path, "ab");
        if (suggestion_log_file == NULL) {
            fprintf(stderr, "stoin: failed to open suggestion log '%s'\n", suggestion_log_path);
            runtime_config_destroy(&runtime_config);
            return 1;
        }
    }

    App app = {
        .steno = create_steno(
            runtime_config.dictionary_paths,
            runtime_config.dictionary_enabled,
            arrlenu(runtime_config.dictionary_paths),
            runtime_config.word_list_path,
            runtime_config.phrasing_path,
            input_mode == INPUT_MODE_QWERTY ? "stoin.keymap" : NULL,
            print_suggestions,
            suggestion_log_file,
            trace_strokes ? stderr : NULL
        ),
        .time_translations = time_translations,
        .trace_key_events = trace_key_events,
        .qwerty_input = input_mode == INPUT_MODE_QWERTY,
        .phrase_toggle_enabled = phrase_toggle_enabled,
        .phrase_toggle_keycode = phrase_toggle_keycode,
        .phrase_toggle_name = phrase_toggle_name,
        .nonverb_phrase_toggle_enabled = nonverb_phrase_toggle_enabled,
        .nonverb_phrase_toggle_keycode = nonverb_phrase_toggle_keycode,
        .nonverb_phrase_toggle_name = nonverb_phrase_toggle_name,
    };
    if (app.steno == NULL) {
        if (suggestion_log_file != NULL) {
            fclose(suggestion_log_file);
        }
        runtime_config_destroy(&runtime_config);
        return 1;
    }
    if (!app_init_phrase_toggle_state(&app)) {
        fputs("stoin: failed to initialize phrase toggle state\n", stderr);
        steno_destroy(app.steno);
        if (suggestion_log_file != NULL) {
            fclose(suggestion_log_file);
        }
        runtime_config_destroy(&runtime_config);
        return 1;
    }
    steno_set_phrase_namespace_enabled(app.steno, app_phrase_namespace_enabled(&app));

    platform_translation_timing_set_enabled(time_translations);
    if (time_translations) {
        fputs("stoin: translation timing enabled; latency stops immediately before the first platform output event\n", stderr);
        if (trace_strokes) {
            fputs("stoin: note: stroke tracing is enabled and included in the measured path; use --no-trace-strokes for cleaner benchmark numbers\n", stderr);
        }
    }

    int status = 0;
    if (input_mode == INPUT_MODE_QWERTY) {
        status = run_qwerty(&app);
    } else {
        if (app_wants_keyboard_events(&app)) {
            if (!platform_init_listen_only(handle_phrase_toggle_input, &app)) {
                app_destroy_phrase_toggle_state(&app);
                steno_destroy(app.steno);
                if (suggestion_log_file != NULL) {
                    fclose(suggestion_log_file);
                }
                runtime_config_destroy(&runtime_config);
                return 1;
            }
            print_phrase_toggle_status(&app);
        }

        if (input_mode == INPUT_MODE_TX_BOLT) {
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
    }

    app_destroy_phrase_toggle_state(&app);
    steno_destroy(app.steno);
    if (suggestion_log_file != NULL && fclose(suggestion_log_file) != 0 && status == 0) {
        status = 1;
    }
    runtime_config_destroy(&runtime_config);
    return status;
}
