#include "stentura.h"

#include "steno_stroke.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define STENTURA_RESPONSE_TIMEOUT_MS 1000
#define STENTURA_MAX_SEND_TRIES 3
#define STENTURA_STENO_BIT(key) (UINT64_C(1) << (uint64_t)(key))

typedef enum Stentura_Read_Result {
    STENTURA_READ_OK,
    STENTURA_READ_TIMEOUT,
    STENTURA_READ_ERROR,
} Stentura_Read_Result;

static const uint64_t STENTURA_BITS[24] = {
    0,
    STENTURA_STENO_BIT(STENO_NUM),
    STENTURA_STENO_BIT(STENO_LEFT_S),
    STENTURA_STENO_BIT(STENO_LEFT_T),
    STENTURA_STENO_BIT(STENO_LEFT_K),
    STENTURA_STENO_BIT(STENO_LEFT_P),

    STENTURA_STENO_BIT(STENO_LEFT_W),
    STENTURA_STENO_BIT(STENO_LEFT_H),
    STENTURA_STENO_BIT(STENO_LEFT_R),
    STENTURA_STENO_BIT(STENO_A),
    STENTURA_STENO_BIT(STENO_O),
    STENTURA_STENO_BIT(STENO_STAR),

    STENTURA_STENO_BIT(STENO_E),
    STENTURA_STENO_BIT(STENO_U),
    STENTURA_STENO_BIT(STENO_RIGHT_F),
    STENTURA_STENO_BIT(STENO_RIGHT_R),
    STENTURA_STENO_BIT(STENO_RIGHT_P),
    STENTURA_STENO_BIT(STENO_RIGHT_B),

    STENTURA_STENO_BIT(STENO_RIGHT_L),
    STENTURA_STENO_BIT(STENO_RIGHT_G),
    STENTURA_STENO_BIT(STENO_RIGHT_T),
    STENTURA_STENO_BIT(STENO_RIGHT_S),
    STENTURA_STENO_BIT(STENO_RIGHT_D),
    STENTURA_STENO_BIT(STENO_RIGHT_Z),
};

static void write_u16le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFF);
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t read_u16le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint8_t next_sequence(Stentura *stentura)
{
    const uint8_t sequence = stentura->next_sequence;
    ++stentura->next_sequence;
    return sequence;
}

static bool dequeue_stroke(Stentura *stentura, uint64_t *out_bits)
{
    if (stentura == NULL || out_bits == NULL || stentura->queued_stroke_count == 0) {
        return false;
    }

    *out_bits = stentura->queued_strokes[0];
    --stentura->queued_stroke_count;
    memmove(
        stentura->queued_strokes,
        stentura->queued_strokes + 1,
        stentura->queued_stroke_count * sizeof(stentura->queued_strokes[0])
    );
    return true;
}

uint16_t stentura_crc16(const uint8_t *data, size_t size)
{
    uint16_t checksum = 0;
    if (data == NULL && size > 0) {
        return checksum;
    }

    for (size_t i = 0; i < size; ++i) {
        checksum ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if ((checksum & 1U) != 0) {
                checksum = (uint16_t)((checksum >> 1) ^ 0xA001U);
            } else {
                checksum >>= 1;
            }
        }
    }

    return checksum;
}

bool stentura_decode_stroke(const uint8_t bytes[4], uint64_t *out_bits)
{
    if (bytes == NULL || out_bits == NULL) {
        return false;
    }

    uint64_t bits = 0;
    for (size_t byte_index = 0; byte_index < 4; ++byte_index) {
        if ((bytes[byte_index] & 0xC0U) != 0xC0U) {
            return false;
        }
        for (size_t bit_index = 0; bit_index < 6; ++bit_index) {
            if ((bytes[byte_index] & (0x20U >> bit_index)) != 0) {
                bits |= STENTURA_BITS[byte_index * 6 + bit_index];
            }
        }
    }

    *out_bits = bits;
    return true;
}

bool stentura_decode_strokes(
    const uint8_t *data,
    size_t data_size,
    uint64_t *out_strokes,
    size_t max_strokes,
    size_t *out_count
)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (data == NULL || out_strokes == NULL || out_count == NULL || (data_size % 4) != 0) {
        return false;
    }

    size_t count = 0;
    for (size_t offset = 0; offset < data_size; offset += 4) {
        uint64_t bits = 0;
        if (!stentura_decode_stroke(data + offset, &bits)) {
            return false;
        }
        if (bits == 0) {
            continue;
        }
        if (count >= max_strokes) {
            return false;
        }
        out_strokes[count++] = bits;
    }

    *out_count = count;
    return true;
}

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
)
{
    const bool has_data = data != NULL && data_size > 0;
    const size_t packet_size = 18 + (has_data ? data_size + 2 : 0);
    if (out_packet == NULL || out_size < packet_size || packet_size > UINT16_MAX) {
        return 0;
    }

    out_packet[0] = 0x01;
    out_packet[1] = sequence;
    write_u16le(out_packet + 2, (uint16_t)packet_size);
    write_u16le(out_packet + 4, (uint16_t)action);
    write_u16le(out_packet + 6, p1);
    write_u16le(out_packet + 8, p2);
    write_u16le(out_packet + 10, p3);
    write_u16le(out_packet + 12, p4);
    write_u16le(out_packet + 14, p5);
    write_u16le(out_packet + 16, stentura_crc16(out_packet + 1, 15));

    if (has_data) {
        memcpy(out_packet + 18, data, data_size);
        write_u16le(out_packet + packet_size - 2, stentura_crc16(data, data_size));
    }

    return packet_size;
}

size_t stentura_make_open(
    uint8_t *out_packet,
    size_t out_size,
    uint8_t sequence,
    char drive,
    const char *filename
)
{
    if (filename == NULL) {
        return 0;
    }
    return stentura_make_request(
        out_packet,
        out_size,
        sequence,
        STENTURA_ACTION_OPEN,
        (uint16_t)(unsigned char)drive,
        0,
        0,
        0,
        0,
        (const uint8_t *)filename,
        strlen(filename)
    );
}

size_t stentura_make_read(
    uint8_t *out_packet,
    size_t out_size,
    uint8_t sequence,
    uint16_t block,
    uint16_t byte,
    uint16_t length
)
{
    return stentura_make_request(
        out_packet,
        out_size,
        sequence,
        STENTURA_ACTION_READC,
        1,
        0,
        length,
        block,
        byte,
        NULL,
        0
    );
}

size_t stentura_make_reset(uint8_t *out_packet, size_t out_size, uint8_t sequence)
{
    return stentura_make_request(
        out_packet,
        out_size,
        sequence,
        STENTURA_ACTION_RESET,
        0,
        0,
        0,
        0,
        0,
        NULL,
        0
    );
}

bool stentura_validate_response(const uint8_t *packet, size_t packet_size)
{
    if (packet == NULL || packet_size < 14 || packet[0] != 0x01) {
        return false;
    }

    const uint16_t declared_size = read_u16le(packet + 2);
    if (declared_size != packet_size) {
        return false;
    }

    if (stentura_crc16(packet + 1, 11) != read_u16le(packet + 12)) {
        return false;
    }

    if (packet_size > 14) {
        if (packet_size < 17) {
            return false;
        }
        const size_t data_size = packet_size - 16;
        if (stentura_crc16(packet + 14, data_size) != read_u16le(packet + packet_size - 2)) {
            return false;
        }
    }

    return true;
}

static Stentura_Read_Result read_bytes(
    Stentura *stentura,
    uint8_t *out_bytes,
    size_t byte_count,
    unsigned int timeout_ms
)
{
    if (stentura == NULL || stentura->serial == NULL || out_bytes == NULL) {
        if (stentura != NULL) {
            stentura->had_error = true;
        }
        errno = EBADF;
        return STENTURA_READ_ERROR;
    }

    const uint64_t started_ms = platform_monotonic_ms();
    const uint64_t deadline_ms = started_ms + timeout_ms;
    for (size_t i = 0; i < byte_count; ++i) {
        const uint64_t now_ms = platform_monotonic_ms();
        if (timeout_ms != 0 && now_ms >= deadline_ms) {
            return STENTURA_READ_TIMEOUT;
        }

        uint64_t remaining_ms = timeout_ms == 0 ? 0 : deadline_ms - now_ms;
        if (remaining_ms > UINT_MAX) {
            remaining_ms = UINT_MAX;
        }

        const Platform_Serial_Read_Result read_result =
            platform_serial_read_byte(stentura->serial, out_bytes + i, (unsigned int)remaining_ms);
        if (read_result == PLATFORM_SERIAL_READ_NONE) {
            return STENTURA_READ_TIMEOUT;
        }
        if (read_result == PLATFORM_SERIAL_READ_ERROR) {
            stentura->had_error = true;
            return STENTURA_READ_ERROR;
        }
    }

    return STENTURA_READ_OK;
}

static Stentura_Read_Result read_packet(
    Stentura *stentura,
    uint8_t *out_packet,
    size_t out_size,
    size_t *out_packet_size
)
{
    if (out_packet_size != NULL) {
        *out_packet_size = 0;
    }
    if (out_packet == NULL || out_size < 14 || out_packet_size == NULL) {
        if (stentura != NULL) {
            stentura->had_error = true;
        }
        errno = EINVAL;
        return STENTURA_READ_ERROR;
    }

    const Stentura_Read_Result header_result =
        read_bytes(stentura, out_packet, 4, STENTURA_RESPONSE_TIMEOUT_MS);
    if (header_result != STENTURA_READ_OK) {
        return header_result;
    }

    const uint16_t packet_size = read_u16le(out_packet + 2);
    if (packet_size < 14 || packet_size > out_size) {
        stentura->had_error = true;
        errno = EPROTO;
        return STENTURA_READ_ERROR;
    }

    const Stentura_Read_Result body_result =
        read_bytes(stentura, out_packet + 4, packet_size - 4, STENTURA_RESPONSE_TIMEOUT_MS);
    if (body_result != STENTURA_READ_OK) {
        return body_result;
    }

    if (!stentura_validate_response(out_packet, packet_size)) {
        stentura->had_error = true;
        errno = EPROTO;
        return STENTURA_READ_ERROR;
    }

    *out_packet_size = packet_size;
    return STENTURA_READ_OK;
}

static bool send_receive(
    Stentura *stentura,
    const uint8_t *request,
    size_t request_size,
    uint8_t *response,
    size_t response_size,
    size_t *out_response_size
)
{
    if (out_response_size != NULL) {
        *out_response_size = 0;
    }
    if (stentura == NULL
        || stentura->serial == NULL
        || request == NULL
        || request_size < 6
        || response == NULL
        || out_response_size == NULL) {
        if (stentura != NULL) {
            stentura->had_error = true;
        }
        errno = EINVAL;
        return false;
    }

    const uint8_t request_sequence = request[1];
    const uint16_t request_action = read_u16le(request + 4);
    for (int attempt = 0; attempt < STENTURA_MAX_SEND_TRIES; ++attempt) {
        if (!platform_serial_write_all(stentura->serial, request, request_size, STENTURA_RESPONSE_TIMEOUT_MS)) {
            stentura->had_error = true;
            return false;
        }

        size_t received_size = 0;
        const Stentura_Read_Result read_result =
            read_packet(stentura, response, response_size, &received_size);
        if (read_result == STENTURA_READ_TIMEOUT) {
            continue;
        }
        if (read_result != STENTURA_READ_OK) {
            stentura->had_error = true;
            return false;
        }
        if (response[1] != request_sequence) {
            continue;
        }
        if (read_u16le(response + 4) != request_action) {
            stentura->had_error = true;
            errno = EPROTO;
            return false;
        }

        *out_response_size = received_size;
        return true;
    }

    stentura->had_error = true;
    errno = ETIMEDOUT;
    return false;
}

static bool read_realtime_data(
    Stentura *stentura,
    uint8_t *out_data,
    size_t out_data_size,
    size_t *out_read_size
)
{
    if (out_read_size != NULL) {
        *out_read_size = 0;
    }
    if (stentura == NULL || out_data == NULL || out_read_size == NULL || out_data_size < STENTURA_READ_SIZE) {
        if (stentura != NULL) {
            stentura->had_error = true;
        }
        errno = EINVAL;
        return false;
    }

    uint8_t request[32] = {0};
    uint8_t response[STENTURA_PACKET_BUFFER_SIZE] = {0};
    const size_t request_size =
        stentura_make_read(request, sizeof(request), next_sequence(stentura), stentura->block, stentura->byte, STENTURA_READ_SIZE);
    if (request_size == 0) {
        stentura->had_error = true;
        errno = EINVAL;
        return false;
    }

    size_t response_size = 0;
    if (!send_receive(stentura, request, request_size, response, sizeof(response), &response_size)) {
        return false;
    }

    const uint16_t p1 = read_u16le(response + 8);
    const size_t response_data_size = response_size > 14 ? response_size - 16 : 0;
    if (!((p1 == 0 && response_size == 14) || p1 == response_data_size)
        || response_data_size > out_data_size) {
        stentura->had_error = true;
        errno = EPROTO;
        return false;
    }

    if (response_data_size > 0) {
        memcpy(out_data, response + 14, response_data_size);
    }

    uint32_t next_byte = (uint32_t)stentura->byte + p1;
    stentura->block = (uint16_t)(stentura->block + (next_byte / STENTURA_READ_SIZE));
    stentura->byte = (uint16_t)(next_byte % STENTURA_READ_SIZE);
    *out_read_size = response_data_size;
    return true;
}

static bool drain_realtime_file(Stentura *stentura)
{
    uint8_t stroke_data[STENTURA_READ_SIZE] = {0};
    while (true) {
        size_t read_size = 0;
        if (!read_realtime_data(stentura, stroke_data, sizeof(stroke_data), &read_size)) {
            return false;
        }
        if (read_size == 0) {
            return true;
        }
    }
}

bool stentura_open(Stentura *stentura, const Stentura_Config *config)
{
    if (stentura == NULL || config == NULL) {
        return false;
    }

    memset(stentura, 0, sizeof(*stentura));

    const int baud_rate = config->baud_rate == 0 ? STENTURA_DEFAULT_BAUD_RATE : config->baud_rate;
    if (!platform_serial_open(&stentura->serial, config->port_path, baud_rate)) {
        return false;
    }

    platform_sleep_ms(STENTURA_RESPONSE_TIMEOUT_MS);
    platform_serial_flush(stentura->serial);

    uint8_t request[64] = {0};
    uint8_t response[STENTURA_PACKET_BUFFER_SIZE] = {0};
    const size_t request_size =
        stentura_make_open(request, sizeof(request), next_sequence(stentura), 'A', "REALTIME.000");
    if (request_size == 0) {
        stentura_close(stentura);
        errno = EINVAL;
        return false;
    }

    size_t response_size = 0;
    if (!send_receive(stentura, request, request_size, response, sizeof(response), &response_size)
        || !drain_realtime_file(stentura)) {
        stentura_close(stentura);
        return false;
    }

    return true;
}

void stentura_close(Stentura *stentura)
{
    if (stentura == NULL) {
        return;
    }

    platform_serial_close(stentura->serial);
    memset(stentura, 0, sizeof(*stentura));
}

const char *stentura_port_path(const Stentura *stentura)
{
    return stentura == NULL ? "" : platform_serial_port_path(stentura->serial);
}

bool stentura_had_error(const Stentura *stentura)
{
    return stentura != NULL && (stentura->had_error || platform_serial_had_error(stentura->serial));
}

bool stentura_read_stroke(Stentura *stentura, uint64_t *out_bits)
{
    if (stentura == NULL || out_bits == NULL || stentura->serial == NULL) {
        return false;
    }

    if (dequeue_stroke(stentura, out_bits)) {
        return true;
    }

    uint8_t stroke_data[STENTURA_READ_SIZE] = {0};
    size_t read_size = 0;
    if (!read_realtime_data(stentura, stroke_data, sizeof(stroke_data), &read_size) || read_size == 0) {
        return false;
    }

    if (!stentura_decode_strokes(
            stroke_data,
            read_size,
            stentura->queued_strokes,
            sizeof(stentura->queued_strokes) / sizeof(stentura->queued_strokes[0]),
            &stentura->queued_stroke_count)) {
        stentura->had_error = true;
        errno = EPROTO;
        return false;
    }

    return dequeue_stroke(stentura, out_bits);
}
