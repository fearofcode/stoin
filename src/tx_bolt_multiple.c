#include "tx_bolt_multiple.h"

#include "platform.h"
#include "stroke_merge.h"
#include "tx_bolt.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TX_BOLT_MULTIPLE_MAX_DEVICES 16
#define TX_BOLT_MULTIPLE_SCAN_INTERVAL_MS 1000
#define TX_BOLT_MULTIPLE_LOOP_SLEEP_MS 1
#define TX_BOLT_STROKE_IDLE_FLUSH_MS 100

typedef struct Multi_Tx_Bolt_Device {
    Tx_Bolt tx_bolt;
    char path[PLATFORM_SERIAL_PATH_MAX];
    uint64_t last_byte_ms;
    int source_id;
} Multi_Tx_Bolt_Device;

static volatile sig_atomic_t g_stop_requested;

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

static bool config_session_active(const Tx_Bolt_Multiple_Config *config)
{
    if (config == NULL || config->session_active == NULL) {
        return true;
    }
    return config->session_active(config->userdata);
}

static bool multi_tx_bolt_path_connected(
    const Multi_Tx_Bolt_Device *devices,
    size_t device_count,
    const char *path
)
{
    for (size_t i = 0; i < device_count; ++i) {
        if (strcmp(devices[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

static void close_multi_tx_bolt_device(Multi_Tx_Bolt_Device *device)
{
    if (device == NULL) {
        return;
    }
    tx_bolt_close(&device->tx_bolt);
    memset(device, 0, sizeof(*device));
}

static void close_multi_tx_bolt_devices(Multi_Tx_Bolt_Device *devices, size_t device_count)
{
    for (size_t i = 0; i < device_count; ++i) {
        close_multi_tx_bolt_device(&devices[i]);
    }
}

static void remove_multi_tx_bolt_device(
    Multi_Tx_Bolt_Device *devices,
    size_t *device_count,
    size_t index
)
{
    if (devices == NULL || device_count == NULL || index >= *device_count) {
        return;
    }

    close_multi_tx_bolt_device(&devices[index]);
    if (index + 1 < *device_count) {
        memmove(
            devices + index,
            devices + index + 1,
            (*device_count - index - 1) * sizeof(devices[0])
        );
    }
    --*device_count;
}

static bool connect_multi_tx_bolt_device(
    Multi_Tx_Bolt_Device *devices,
    size_t *device_count,
    const char *path,
    int baud_rate,
    int *next_source_id
)
{
    if (devices == NULL || device_count == NULL || path == NULL || next_source_id == NULL) {
        return false;
    }
    if (*device_count >= TX_BOLT_MULTIPLE_MAX_DEVICES
        || multi_tx_bolt_path_connected(devices, *device_count, path)) {
        return false;
    }

    Multi_Tx_Bolt_Device *device = &devices[*device_count];
    memset(device, 0, sizeof(*device));

    const Tx_Bolt_Config tx_bolt_config = {
        .port_path = path,
        .baud_rate = baud_rate,
    };
    if (!tx_bolt_open(&device->tx_bolt, &tx_bolt_config)) {
        memset(device, 0, sizeof(*device));
        return false;
    }

    const int written = snprintf(device->path, sizeof(device->path), "%s", path);
    if (written <= 0 || (size_t)written >= sizeof(device->path)) {
        tx_bolt_close(&device->tx_bolt);
        memset(device, 0, sizeof(*device));
        return false;
    }

    device->source_id = (*next_source_id)++;
    ++*device_count;
    printf("stoin: TX Bolt connected on %s\n", path);
    return true;
}

static void scan_multi_tx_bolt_devices(
    Multi_Tx_Bolt_Device *devices,
    size_t *device_count,
    const char *port_path,
    int baud_rate,
    int *next_source_id,
    bool *announced_disconnected
)
{
    if (devices == NULL || device_count == NULL || next_source_id == NULL) {
        return;
    }

    if (port_path != NULL) {
        if (!multi_tx_bolt_path_connected(devices, *device_count, port_path)) {
            (void)connect_multi_tx_bolt_device(devices, device_count, port_path, baud_rate, next_source_id);
        }
    } else {
        char paths[TX_BOLT_MULTIPLE_MAX_DEVICES][PLATFORM_SERIAL_PATH_MAX] = {{0}};
        const size_t path_count = platform_find_serial_devices(paths, TX_BOLT_MULTIPLE_MAX_DEVICES);
        for (size_t i = 0; i < path_count; ++i) {
            (void)connect_multi_tx_bolt_device(devices, device_count, paths[i], baud_rate, next_source_id);
        }
    }

    if (*device_count == 0) {
        if (announced_disconnected != NULL && !*announced_disconnected) {
            if (port_path != NULL) {
                fprintf(stderr, "stoin: TX Bolt disconnected; waiting for %s\n", port_path);
            } else {
                fputs("stoin: TX Bolt disconnected; waiting for platform serial devices\n", stderr);
            }
            *announced_disconnected = true;
        }
    } else if (announced_disconnected != NULL) {
        *announced_disconnected = false;
    }
}

static void process_merge_outputs(
    const Tx_Bolt_Multiple_Config *config,
    Stroke_Merge *merge,
    bool session_active
)
{
    if (config == NULL || merge == NULL) {
        return;
    }

    uint64_t stroke_bits = 0;
    while (stroke_merge_next_output(merge, &stroke_bits)) {
        if (session_active && config->handle_stroke != NULL) {
            (void)config->handle_stroke(stroke_bits, platform_monotonic_ns(), config->userdata);
        }
    }
}

static void push_multi_tx_bolt_stroke(
    const Tx_Bolt_Multiple_Config *config,
    Stroke_Merge *merge,
    int source_id,
    uint64_t stroke_bits,
    unsigned int window_ms,
    bool session_active,
    uint64_t now_ms
)
{
    if (config == NULL || merge == NULL) {
        return;
    }
    if (!session_active) {
        stroke_merge_clear(merge);
        return;
    }

    stroke_merge_set_window_ms(merge, window_ms);
    (void)stroke_merge_push(merge, source_id, stroke_bits, now_ms);
    process_merge_outputs(config, merge, session_active);
}

int tx_bolt_multiple_run(const Tx_Bolt_Multiple_Config *config)
{
    if (config == NULL) {
        return 1;
    }

    g_stop_requested = 0;
    signal(SIGINT, request_stop);

    const int resolved_baud_rate =
        config->baud_rate == 0 ? TX_BOLT_DEFAULT_BAUD_RATE : config->baud_rate;
    printf("stoin: TX Bolt multiple-input serial capture starting at %d baud 8N1\n", resolved_baud_rate);
    printf("stoin: multiple-input merge window is %u ms when 2+ devices are connected\n",
        config->merge_window_ms);
    printf("stoin: loaded %zu dictionary entries\n", config->dictionary_count);
    puts("stoin: press Ctrl+C in this terminal to quit");
    if (config->port_path != NULL) {
        fputs("stoin: --serial-port limits multiple-input mode to that single port\n", stderr);
    }

    Multi_Tx_Bolt_Device devices[TX_BOLT_MULTIPLE_MAX_DEVICES] = {0};
    size_t device_count = 0;
    int next_source_id = 1;
    bool output_ready = false;
    bool announced_disconnected = false;
    uint64_t next_scan_ms = 0;
    Stroke_Merge merge = {0};
    stroke_merge_init(&merge, config->merge_window_ms);
    if (config->start_watcher != NULL) {
        config->start_watcher(config->userdata);
    }

    while (!g_stop_requested) {
        if (config->run_maintenance != NULL) {
            config->run_maintenance(config->userdata);
        }
        const bool session_active = config_session_active(config);
        uint64_t now_ms = platform_monotonic_ms();

        if (!session_active) {
            stroke_merge_clear(&merge);
        }

        if (now_ms >= next_scan_ms) {
            scan_multi_tx_bolt_devices(
                devices,
                &device_count,
                config->port_path,
                resolved_baud_rate,
                &next_source_id,
                &announced_disconnected
            );
            next_scan_ms = now_ms + TX_BOLT_MULTIPLE_SCAN_INTERVAL_MS;
        }

        if (device_count > 0 && !output_ready) {
            if (!platform_output_init()) {
                close_multi_tx_bolt_devices(devices, device_count);
                stroke_merge_destroy(&merge);
                platform_shutdown();
                return 1;
            }
            output_ready = true;
        }

        const unsigned int active_window_ms = device_count > 1 ? config->merge_window_ms : 0;
        stroke_merge_set_window_ms(&merge, session_active ? active_window_ms : 0);
        (void)stroke_merge_poll(&merge, now_ms);
        process_merge_outputs(config, &merge, session_active);

        bool made_progress = false;
        for (size_t i = 0; i < device_count;) {
            Multi_Tx_Bolt_Device *device = &devices[i];
            bool remove_device = false;

            while (true) {
                uint64_t stroke_bits = 0;
                bool read_byte = false;
                if (tx_bolt_read_stroke_nonblocking(&device->tx_bolt, &stroke_bits, &read_byte)) {
                    now_ms = platform_monotonic_ms();
                    if (read_byte) {
                        device->last_byte_ms = now_ms;
                    }
                    push_multi_tx_bolt_stroke(
                        config,
                        &merge,
                        device->source_id,
                        stroke_bits,
                        active_window_ms,
                        session_active,
                        now_ms
                    );
                    made_progress = true;
                    continue;
                }

                if (read_byte) {
                    device->last_byte_ms = platform_monotonic_ms();
                    made_progress = true;
                    continue;
                }
                break;
            }

            if (tx_bolt_had_error(&device->tx_bolt)) {
                remove_device = true;
            } else {
                now_ms = platform_monotonic_ms();
                if (tx_bolt_has_partial_stroke(&device->tx_bolt)
                    && device->last_byte_ms != 0
                    && now_ms - device->last_byte_ms >= TX_BOLT_STROKE_IDLE_FLUSH_MS) {
                    uint64_t stroke_bits = 0;
                    device->last_byte_ms = 0;
                    if (tx_bolt_flush_stroke(&device->tx_bolt, &stroke_bits)) {
                        push_multi_tx_bolt_stroke(
                            config,
                            &merge,
                            device->source_id,
                            stroke_bits,
                            active_window_ms,
                            session_active,
                            now_ms
                        );
                        made_progress = true;
                    }
                    if (tx_bolt_had_error(&device->tx_bolt)) {
                        remove_device = true;
                    }
                }
            }

            if (remove_device) {
                printf("stoin: TX Bolt disconnected from %s; waiting for reconnect\n", device->path);
                remove_multi_tx_bolt_device(devices, &device_count, i);
                stroke_merge_set_window_ms(&merge, device_count > 1 ? config->merge_window_ms : 0);
                made_progress = true;
                continue;
            }

            ++i;
        }

        now_ms = platform_monotonic_ms();
        (void)stroke_merge_poll(&merge, now_ms);
        process_merge_outputs(config, &merge, session_active);

        if (!made_progress) {
            platform_sleep_ms(TX_BOLT_MULTIPLE_LOOP_SLEEP_MS);
        }
    }

    stroke_merge_set_window_ms(&merge, 0);
    process_merge_outputs(config, &merge, config_session_active(config));
    close_multi_tx_bolt_devices(devices, device_count);
    stroke_merge_destroy(&merge);
    platform_shutdown();
    return 0;
}
