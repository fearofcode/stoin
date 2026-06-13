#ifndef TX_BOLT_H
#define TX_BOLT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TX_BOLT_DEFAULT_BAUD_RATE 9600

typedef struct Tx_Bolt {
    int fd;
    char port_path[256];
    uint64_t stroke_bits;
    uint64_t queued_strokes[4];
    size_t queued_stroke_count;
    int last_key_set;
    bool had_error;
} Tx_Bolt;

typedef struct Tx_Bolt_Config {
    const char *port_path;
    int baud_rate;
} Tx_Bolt_Config;

bool tx_bolt_open(Tx_Bolt *tx_bolt, const Tx_Bolt_Config *config);
void tx_bolt_close(Tx_Bolt *tx_bolt);
const char *tx_bolt_port_path(const Tx_Bolt *tx_bolt);
bool tx_bolt_had_error(const Tx_Bolt *tx_bolt);
bool tx_bolt_read_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits);
bool tx_bolt_decode_byte(Tx_Bolt *tx_bolt, uint8_t byte, uint64_t *out_bits);
bool tx_bolt_flush_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits);

#endif
