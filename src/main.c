#include "gemini_pr.h"
#include "platform.h"
#include "steno.h"
#include "tx_bolt.h"
#include "util.h"

#include <ctype.h>
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
} Input_Mode;

#define DEFAULT_DICTIONARY_PATH "stoin-dictionary.json"
#define DEFAULT_CONFIG_PATH "stoin-config.json"
#define DEFAULT_WORD_LIST_PATH "american_english_words.txt"

typedef struct Runtime_Config {
    char **dictionary_paths;
    char *word_list_path;
} Runtime_Config;

typedef struct App {
    Steno *steno;
    bool session_active;
    bool session_state_known;
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
                "stoin: macOS user session %s; steno capture %s\n",
                active ? "active" : "inactive",
                active ? "resumed" : "suspended");
        } else if (!active) {
            fputs("stoin: macOS user session inactive; steno capture suspended until login\n", stderr);
        }
        app->session_active = active;
        app->session_state_known = true;
    }

    return active;
}

static bool handle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;
    if (!update_session_active(app)) {
        return false;
    }
    return steno_handle_event(app->steno, event);
}

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

static void print_usage(void)
{
    fputs("usage: stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH]\n", stderr);
    fputs("             [--input tx-bolt|gemini-pr|qwerty]\n", stderr);
    fputs("             [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("             [--trace-strokes|--no-trace-strokes]\n", stderr);
    fputs("       stoin --raw-serial [--serial-port PATH] [--serial-baud BAUD]\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] --lookup STROKE\n", stderr);
    fputs("       stoin [--config PATH] [--dictionary PATH ...] [--word-list PATH] --dump-dictionary [OUTPUT_PATH]\n", stderr);
}

static const char *skip_json_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static bool parse_json_string(const char **cursor, char **out_string)
{
    const char *p = *cursor;
    if (*p != '"') {
        return false;
    }
    ++p;

    char *result = NULL;
    while (*p != '\0' && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            switch (c) {
            case '"': arrput(result, '"'); break;
            case '\\': arrput(result, '\\'); break;
            case '/': arrput(result, '/'); break;
            case 'b': arrput(result, '\b'); break;
            case 'f': arrput(result, '\f'); break;
            case 'n': arrput(result, '\n'); break;
            case 'r': arrput(result, '\r'); break;
            case 't': arrput(result, '\t'); break;
            default:
                arrfree(result);
                return false;
            }
        } else {
            arrput(result, (char)c);
        }
    }

    if (*p != '"') {
        arrfree(result);
        return false;
    }
    ++p;

    arrput(result, '\0');
    *cursor = p;
    *out_string = result;
    return true;
}

static void runtime_config_clear_dictionaries(Runtime_Config *config)
{
    if (config == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(config->dictionary_paths); ++i) {
        free(config->dictionary_paths[i]);
    }
    arrfree(config->dictionary_paths);
    config->dictionary_paths = NULL;
}

static bool runtime_config_add_dictionary(Runtime_Config *config, const char *path)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    arrput(config->dictionary_paths, copy);
    return true;
}

static bool runtime_config_set_word_list(Runtime_Config *config, const char *path)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    free(config->word_list_path);
    config->word_list_path = copy;
    return true;
}

static void runtime_config_destroy(Runtime_Config *config)
{
    if (config == NULL) {
        return;
    }
    runtime_config_clear_dictionaries(config);
    free(config->word_list_path);
    memset(config, 0, sizeof(*config));
}

static bool runtime_config_parse_dictionary_array(Runtime_Config *config, const char **cursor)
{
    const char *p = skip_json_ws(*cursor);
    if (*p != '[') {
        return false;
    }
    ++p;

    runtime_config_clear_dictionaries(config);
    while (true) {
        p = skip_json_ws(p);
        if (*p == ']') {
            *cursor = p + 1;
            return true;
        }

        char *path = NULL;
        if (!parse_json_string(&p, &path)) {
            return false;
        }
        const bool ok = runtime_config_add_dictionary(config, path);
        arrfree(path);
        if (!ok) {
            return false;
        }

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == ']') {
            *cursor = p + 1;
            return true;
        }
        return false;
    }
}

static bool runtime_config_load(Runtime_Config *config, const char *path, bool missing_ok)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        if (missing_ok) {
            return true;
        }
        fprintf(stderr, "stoin: failed to read config '%s'\n", path);
        return false;
    }

    const char *p = skip_json_ws(file);
    if (*p != '{') {
        fprintf(stderr, "stoin: config '%s' is not a JSON object\n", path);
        free(file);
        return false;
    }
    ++p;

    bool parsed_ok = false;
    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            parsed_ok = true;
            break;
        }

        char *key = NULL;
        if (!parse_json_string(&p, &key)) {
            break;
        }
        p = skip_json_ws(p);
        if (*p != ':') {
            arrfree(key);
            break;
        }
        ++p;

        bool value_ok = false;
        if (strcmp(key, "word_list") == 0) {
            char *word_list = NULL;
            p = skip_json_ws(p);
            value_ok = parse_json_string(&p, &word_list)
                && runtime_config_set_word_list(config, word_list);
            arrfree(word_list);
        } else if (strcmp(key, "dictionaries") == 0) {
            value_ok = runtime_config_parse_dictionary_array(config, &p);
        }
        arrfree(key);
        if (!value_ok) {
            break;
        }

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            parsed_ok = true;
            break;
        }
        break;
    }

    free(file);
    if (!parsed_ok) {
        fprintf(stderr, "stoin: config '%s' could not be parsed\n", path);
        return false;
    }
    return true;
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
    (void)update_session_active(app);

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

    while (!g_stop_requested) {
        const bool session_active = update_session_active(app);

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
                (void)steno_handle_stroke_bits(app->steno, stroke_bits);
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

    while (!g_stop_requested) {
        const bool session_active = update_session_active(app);

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
                (void)steno_handle_stroke_bits(app->steno, stroke_bits);
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

static void print_raw_serial_burst(const uint8_t *bytes, size_t count)
{
    if (bytes == NULL || count == 0) {
        return;
    }

    printf("stoin: raw %zu byte%s:", count, count == 1 ? "" : "s");
    for (size_t i = 0; i < count; ++i) {
        printf(" %02X", bytes[i]);
    }

    fputs(" | ", stdout);
    for (size_t i = 0; i < count; ++i) {
        const unsigned char ch = bytes[i];
        fputc(isprint(ch) ? ch : '.', stdout);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

static int run_raw_serial(const char *port_path, int baud_rate)
{
    enum {
        RAW_SERIAL_BURST_CAPACITY = 256,
    };

    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    const int resolved_baud_rate = baud_rate == 0 ? TX_BOLT_DEFAULT_BAUD_RATE : baud_rate;
    printf("stoin: raw serial dump starting at %d baud 8N1\n", resolved_baud_rate);
    puts("stoin: dictionary, text output, and keyboard capture are disabled in this mode");
    puts("stoin: press Ctrl+C in this terminal to quit");

    Platform_Serial_Port *serial = NULL;
    bool announced_disconnected = false;
    uint8_t burst[RAW_SERIAL_BURST_CAPACITY];
    size_t burst_count = 0;

    while (!g_stop_requested) {
        if (serial == NULL) {
            char resolved_port_path[256] = {0};
            if (!resolve_serial_port(port_path, resolved_port_path, sizeof(resolved_port_path))) {
                if (!announced_disconnected) {
                    if (port_path != NULL) {
                        fprintf(stderr, "stoin: raw serial disconnected; waiting for %s\n", port_path);
                    } else {
                        fputs("stoin: raw serial disconnected; waiting for a platform default serial device\n", stderr);
                    }
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            errno = 0;
            if (!platform_serial_open(&serial, resolved_port_path, resolved_baud_rate)) {
                if (!announced_disconnected) {
                    fprintf(stderr, "stoin: raw serial disconnected; waiting for %s", resolved_port_path);
                    if (errno != 0) {
                        fprintf(stderr, " (%s)", strerror(errno));
                    }
                    fputc('\n', stderr);
                    announced_disconnected = true;
                }
                platform_sleep_ms(1000);
                continue;
            }

            announced_disconnected = false;
            printf("stoin: raw serial connected on %s\n", platform_serial_port_path(serial));
        }

        uint8_t byte = 0;
        const Platform_Serial_Read_Result read_result = platform_serial_read_byte(serial, &byte, 100);
        if (read_result == PLATFORM_SERIAL_READ_BYTE) {
            if (burst_count == sizeof(burst)) {
                print_raw_serial_burst(burst, burst_count);
                burst_count = 0;
            }
            burst[burst_count++] = byte;
        } else if (read_result == PLATFORM_SERIAL_READ_NONE) {
            if (burst_count > 0) {
                print_raw_serial_burst(burst, burst_count);
                burst_count = 0;
            }
        } else {
            if (burst_count > 0) {
                print_raw_serial_burst(burst, burst_count);
                burst_count = 0;
            }
            if (platform_serial_had_error(serial)) {
                printf("stoin: raw serial disconnected from %s; waiting for reconnect\n", platform_serial_port_path(serial));
            }
            platform_serial_close(serial);
            serial = NULL;
            platform_sleep_ms(1000);
        }
    }

    if (burst_count > 0) {
        print_raw_serial_burst(burst, burst_count);
    }
    if (serial != NULL) {
        platform_serial_close(serial);
    }
    return 0;
}

static Steno *create_steno(
    char *const *dictionary_paths,
    size_t dictionary_path_count,
    const char *word_list_path,
    const char *keymap_path,
    FILE *trace_file
)
{
    const Steno_Config steno_config = {
        .keymap_path = keymap_path,
        .dictionary_paths = (const char *const *)dictionary_paths,
        .dictionary_path_count = dictionary_path_count,
        .word_list_path = word_list_path,
        .send_text = send_text,
        .delete_text = delete_text,
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
    bool trace_strokes = true;

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
            } else {
                fprintf(stderr, "stoin: unknown input mode '%s'\n", argv[i]);
                print_usage();
                runtime_config_destroy(&runtime_config);
                return 1;
            }
        } else if ((strcmp(argv[i], "--serial-port") == 0
                || strcmp(argv[i], "--port") == 0
                || strcmp(argv[i], "--tx-bolt-port") == 0
                || strcmp(argv[i], "--gemini-port") == 0) && i + 1 < argc) {
            serial_port = argv[++i];
        } else if ((strcmp(argv[i], "--serial-baud") == 0
                || strcmp(argv[i], "--baud") == 0
                || strcmp(argv[i], "--tx-bolt-baud") == 0
                || strcmp(argv[i], "--gemini-baud") == 0) && i + 1 < argc) {
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

    if (raw_serial_dump) {
        const int status = run_raw_serial(serial_port, serial_baud_rate);
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
            arrlenu(runtime_config.dictionary_paths),
            runtime_config.word_list_path,
            input_mode == INPUT_MODE_QWERTY ? "stoin.keymap" : NULL,
            trace_strokes ? stderr : NULL
        ),
    };
    if (app.steno == NULL) {
        runtime_config_destroy(&runtime_config);
        return 1;
    }

    int status = 0;
    if (input_mode == INPUT_MODE_QWERTY) {
        status = run_qwerty(&app);
    } else if (input_mode == INPUT_MODE_TX_BOLT) {
        status = run_tx_bolt(&app, serial_port, serial_baud_rate);
    } else {
        status = run_gemini_pr(&app, serial_port, serial_baud_rate);
    }

    steno_destroy(app.steno);
    runtime_config_destroy(&runtime_config);
    return status;
}
