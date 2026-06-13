#ifndef GEMINI_PR_H
#define GEMINI_PR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GEMINI_PR_DEFAULT_BAUD_RATE 9600
#define GEMINI_PR_PACKET_SIZE 6

typedef struct Gemini_Pr {
    int fd;
    char port_path[256];
    int packet_index;
    uint8_t packet[GEMINI_PR_PACKET_SIZE];
    bool had_error;
} Gemini_Pr;

typedef struct Gemini_Pr_Config {
    const char *port_path;
    int baud_rate;
} Gemini_Pr_Config;

bool gemini_pr_open(Gemini_Pr *gemini, const Gemini_Pr_Config *config);
void gemini_pr_close(Gemini_Pr *gemini);
const char *gemini_pr_port_path(const Gemini_Pr *gemini);
bool gemini_pr_had_error(const Gemini_Pr *gemini);
bool gemini_pr_read_stroke(Gemini_Pr *gemini, uint64_t *out_bits);
bool gemini_pr_decode_packet(const uint8_t packet[GEMINI_PR_PACKET_SIZE], uint64_t *out_bits);

#endif
