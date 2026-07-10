#ifndef STENTURA_H
#define STENTURA_H

#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STENTURA_DEFAULT_BAUD_RATE 9600
#define STENTURA_READ_SIZE 512
#define STENTURA_STROKES_PER_READ (STENTURA_READ_SIZE / 4)
#define STENTURA_PACKET_BUFFER_SIZE 1024

typedef enum Stentura_Action {
    STENTURA_ACTION_OPEN = 0x0A,
    STENTURA_ACTION_READC = 0x0B,
    STENTURA_ACTION_RESET = 0x14,
} Stentura_Action;

typedef struct Stentura {
    Platform_Serial_Port *serial;
    uint8_t next_sequence;
    uint16_t block;
    uint16_t byte;
    uint64_t queued_strokes[STENTURA_STROKES_PER_READ];
    size_t queued_stroke_count;
    bool had_error;
} Stentura;

typedef struct Stentura_Config {
    const char *port_path;
    int baud_rate;
} Stentura_Config;

bool stentura_open(Stentura *stentura, const Stentura_Config *config);
void stentura_close(Stentura *stentura);
const char *stentura_port_path(const Stentura *stentura);
bool stentura_had_error(const Stentura *stentura);
bool stentura_read_stroke(Stentura *stentura, uint64_t *out_bits);

uint16_t stentura_crc16(const uint8_t *data, size_t size);
bool stentura_decode_stroke(const uint8_t bytes[4], uint64_t *out_bits);
bool stentura_decode_strokes(
    const uint8_t *data,
    size_t data_size,
    uint64_t *out_strokes,
    size_t max_strokes,
    size_t *out_count
);
size_t stentura_make_request(
    uint8_t *out_packet,
    size_t out_size,
    uint8_t sequence,
    Stentura_Action action,
    uint16_t p1,
    uint16_t p2,
    uint16_t p3,
    uint16_t p4,
    uint16_t p5,
    const uint8_t *data,
    size_t data_size
);
size_t stentura_make_open(
    uint8_t *out_packet,
    size_t out_size,
    uint8_t sequence,
    char drive,
    const char *filename
);
size_t stentura_make_read(
    uint8_t *out_packet,
    size_t out_size,
    uint8_t sequence,
    uint16_t block,
    uint16_t byte,
    uint16_t length
);
size_t stentura_make_reset(uint8_t *out_packet, size_t out_size, uint8_t sequence);
bool stentura_validate_response(const uint8_t *packet, size_t packet_size);

#endif
