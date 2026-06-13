#ifndef PROCAT_H
#define PROCAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCAT_DEFAULT_BAUD_RATE 9600
#define PROCAT_PACKET_SIZE 4

typedef struct Procat {
    int fd;
    char port_path[256];
    int packet_index;
    uint8_t packet[PROCAT_PACKET_SIZE];
    bool had_error;
} Procat;

typedef struct Procat_Config {
    const char *port_path;
    int baud_rate;
} Procat_Config;

bool procat_open(Procat *procat, const Procat_Config *config);
void procat_close(Procat *procat);
const char *procat_port_path(const Procat *procat);
bool procat_had_error(const Procat *procat);
bool procat_read_stroke(Procat *procat, uint64_t *out_bits);
bool procat_decode_packet(const uint8_t packet[PROCAT_PACKET_SIZE], uint64_t *out_bits);

#endif
