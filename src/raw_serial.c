#include "raw_serial.h"

#include "platform.h"
#include "tx_bolt.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t g_stop_requested;

static void request_stop(int signum)
{
    (void)signum;
    g_stop_requested = 1;
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

int raw_serial_run(const char *port_path, int baud_rate)
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
            char resolved_port_path[PLATFORM_SERIAL_PATH_MAX] = {0};
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
