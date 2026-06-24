#ifndef TX_BOLT_MULTIPLE_H
#define TX_BOLT_MULTIPLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TX_BOLT_MULTIPLE_DEFAULT_WINDOW_MS 150

typedef void (*Tx_Bolt_Multiple_Callback)(void *userdata);
typedef bool (*Tx_Bolt_Multiple_Session_Active_Fn)(void *userdata);
typedef bool (*Tx_Bolt_Multiple_Handle_Stroke_Fn)(uint64_t bits, uint64_t received_ns, void *userdata);

typedef struct Tx_Bolt_Multiple_Config {
    const char *port_path;
    int baud_rate;
    unsigned int merge_window_ms;
    size_t dictionary_count;
    Tx_Bolt_Multiple_Callback start_watcher;
    Tx_Bolt_Multiple_Callback run_maintenance;
    Tx_Bolt_Multiple_Session_Active_Fn session_active;
    Tx_Bolt_Multiple_Handle_Stroke_Fn handle_stroke;
    void *userdata;
} Tx_Bolt_Multiple_Config;

int tx_bolt_multiple_run(const Tx_Bolt_Multiple_Config *config);

#endif
