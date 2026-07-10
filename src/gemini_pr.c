#include "gemini_pr.h"

#include "steno_stroke.h"

#include <string.h>

#define GEMINI_STENO_BIT(key) (UINT64_C(1) << (uint64_t)(key))

static const uint64_t GEMINI_PR_BITS[GEMINI_PR_PACKET_SIZE * 7] = {
    0, GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM),
    GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM),

    GEMINI_STENO_BIT(STENO_LEFT_S), GEMINI_STENO_BIT(STENO_LEFT_S), GEMINI_STENO_BIT(STENO_LEFT_T),
    GEMINI_STENO_BIT(STENO_LEFT_K), GEMINI_STENO_BIT(STENO_LEFT_P), GEMINI_STENO_BIT(STENO_LEFT_W),
    GEMINI_STENO_BIT(STENO_LEFT_H),

    GEMINI_STENO_BIT(STENO_LEFT_R), GEMINI_STENO_BIT(STENO_A), GEMINI_STENO_BIT(STENO_O),
    GEMINI_STENO_BIT(STENO_STAR), GEMINI_STENO_BIT(STENO_STAR), 0, 0,

    0, GEMINI_STENO_BIT(STENO_STAR), GEMINI_STENO_BIT(STENO_STAR), GEMINI_STENO_BIT(STENO_E),
    GEMINI_STENO_BIT(STENO_U), GEMINI_STENO_BIT(STENO_RIGHT_F), GEMINI_STENO_BIT(STENO_RIGHT_R),

    GEMINI_STENO_BIT(STENO_RIGHT_P), GEMINI_STENO_BIT(STENO_RIGHT_B), GEMINI_STENO_BIT(STENO_RIGHT_L),
    GEMINI_STENO_BIT(STENO_RIGHT_G), GEMINI_STENO_BIT(STENO_RIGHT_T), GEMINI_STENO_BIT(STENO_RIGHT_S),
    GEMINI_STENO_BIT(STENO_RIGHT_D),

    GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM),
    GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM), GEMINI_STENO_BIT(STENO_NUM),
    GEMINI_STENO_BIT(STENO_RIGHT_Z),
};

bool gemini_pr_open(Gemini_Pr *gemini, const Gemini_Pr_Config *config)
{
    if (gemini == NULL || config == NULL) {
        return false;
    }

    memset(gemini, 0, sizeof(*gemini));

    const int baud_rate = config->baud_rate == 0 ? GEMINI_PR_DEFAULT_BAUD_RATE : config->baud_rate;
    return platform_serial_open(&gemini->serial, config->port_path, baud_rate);
}

void gemini_pr_close(Gemini_Pr *gemini)
{
    if (gemini == NULL) {
        return;
    }
    platform_serial_close(gemini->serial);
    gemini->serial = NULL;
    gemini->packet_index = 0;
}

const char *gemini_pr_port_path(const Gemini_Pr *gemini)
{
    return gemini == NULL ? "" : platform_serial_port_path(gemini->serial);
}

bool gemini_pr_had_error(const Gemini_Pr *gemini)
{
    return gemini != NULL && platform_serial_had_error(gemini->serial);
}

bool gemini_pr_decode_packet(const uint8_t packet[GEMINI_PR_PACKET_SIZE], uint64_t *out_bits)
{
    if (packet == NULL || out_bits == NULL) {
        return false;
    }
    if ((packet[0] & 0x80) == 0) {
        return false;
    }
    for (size_t i = 1; i < GEMINI_PR_PACKET_SIZE; ++i) {
        if ((packet[i] & 0x80) != 0) {
            return false;
        }
    }

    uint64_t bits = 0;
    for (size_t byte_index = 0; byte_index < GEMINI_PR_PACKET_SIZE; ++byte_index) {
        for (size_t bit_index = 0; bit_index < 7; ++bit_index) {
            if ((packet[byte_index] & (0x40 >> bit_index)) != 0) {
                bits |= GEMINI_PR_BITS[byte_index * 7 + bit_index];
            }
        }
    }

    if (bits == 0) {
        return false;
    }

    *out_bits = bits;
    return true;
}

bool gemini_pr_read_stroke(Gemini_Pr *gemini, uint64_t *out_bits)
{
    if (gemini == NULL || out_bits == NULL || gemini->serial == NULL) {
        return false;
    }

    while (!platform_serial_had_error(gemini->serial)) {
        uint8_t byte = 0;
        const Platform_Serial_Read_Result read_result =
            platform_serial_read_byte(gemini->serial, &byte, 100);
        if (read_result != PLATFORM_SERIAL_READ_BYTE) {
            return false;
        }

        if (gemini->packet_index == 0) {
            if ((byte & 0x80) == 0) {
                continue;
            }
            gemini->packet[gemini->packet_index++] = byte;
            continue;
        }

        if ((byte & 0x80) != 0) {
            gemini->packet[0] = byte;
            gemini->packet_index = 1;
            continue;
        }

        gemini->packet[gemini->packet_index++] = byte;
        if (gemini->packet_index == GEMINI_PR_PACKET_SIZE) {
            gemini->packet_index = 0;
            if (gemini_pr_decode_packet(gemini->packet, out_bits)) {
                return true;
            }
        }
    }

    return false;
}
