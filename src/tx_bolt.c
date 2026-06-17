#include "tx_bolt.h"

#include "steno_stroke.h"

#include <errno.h>
#include <string.h>

#define TX_BOLT_STENO_BIT(key) (UINT64_C(1) << (uint64_t)(key))

static const uint64_t TX_BOLT_BITS[4][6] = {
    {
        TX_BOLT_STENO_BIT(STENO_LEFT_S),
        TX_BOLT_STENO_BIT(STENO_LEFT_T),
        TX_BOLT_STENO_BIT(STENO_LEFT_K),
        TX_BOLT_STENO_BIT(STENO_LEFT_P),
        TX_BOLT_STENO_BIT(STENO_LEFT_W),
        TX_BOLT_STENO_BIT(STENO_LEFT_H),
    },
    {
        TX_BOLT_STENO_BIT(STENO_LEFT_R),
        TX_BOLT_STENO_BIT(STENO_A),
        TX_BOLT_STENO_BIT(STENO_O),
        TX_BOLT_STENO_BIT(STENO_STAR),
        TX_BOLT_STENO_BIT(STENO_E),
        TX_BOLT_STENO_BIT(STENO_U),
    },
    {
        TX_BOLT_STENO_BIT(STENO_RIGHT_F),
        TX_BOLT_STENO_BIT(STENO_RIGHT_R),
        TX_BOLT_STENO_BIT(STENO_RIGHT_P),
        TX_BOLT_STENO_BIT(STENO_RIGHT_B),
        TX_BOLT_STENO_BIT(STENO_RIGHT_L),
        TX_BOLT_STENO_BIT(STENO_RIGHT_G),
    },
    {
        TX_BOLT_STENO_BIT(STENO_RIGHT_T),
        TX_BOLT_STENO_BIT(STENO_RIGHT_S),
        TX_BOLT_STENO_BIT(STENO_RIGHT_D),
        TX_BOLT_STENO_BIT(STENO_RIGHT_Z),
        TX_BOLT_STENO_BIT(STENO_NUM),
        0,
    },
};

static void reset_stroke(Tx_Bolt *tx_bolt)
{
    tx_bolt->stroke_bits = 0;
    tx_bolt->last_key_set = 0;
}

static bool enqueue_stroke(Tx_Bolt *tx_bolt, uint64_t bits)
{
    if (bits == 0) {
        return true;
    }
    if (tx_bolt->queued_stroke_count >= sizeof(tx_bolt->queued_strokes) / sizeof(tx_bolt->queued_strokes[0])) {
        tx_bolt->had_error = true;
        errno = EOVERFLOW;
        return false;
    }
    tx_bolt->queued_strokes[tx_bolt->queued_stroke_count++] = bits;
    return true;
}

static bool dequeue_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits)
{
    if (tx_bolt->queued_stroke_count == 0 || out_bits == NULL) {
        return false;
    }

    *out_bits = tx_bolt->queued_strokes[0];
    --tx_bolt->queued_stroke_count;
    memmove(
        tx_bolt->queued_strokes,
        tx_bolt->queued_strokes + 1,
        tx_bolt->queued_stroke_count * sizeof(tx_bolt->queued_strokes[0])
    );
    return true;
}

static bool finish_current_stroke(Tx_Bolt *tx_bolt)
{
    const uint64_t bits = tx_bolt->stroke_bits;
    reset_stroke(tx_bolt);
    return enqueue_stroke(tx_bolt, bits);
}

bool tx_bolt_open(Tx_Bolt *tx_bolt, const Tx_Bolt_Config *config)
{
    if (tx_bolt == NULL || config == NULL) {
        return false;
    }

    memset(tx_bolt, 0, sizeof(*tx_bolt));

    const int baud_rate = config->baud_rate == 0 ? TX_BOLT_DEFAULT_BAUD_RATE : config->baud_rate;
    return platform_serial_open(&tx_bolt->serial, config->port_path, baud_rate);
}

void tx_bolt_close(Tx_Bolt *tx_bolt)
{
    if (tx_bolt == NULL) {
        return;
    }
    platform_serial_close(tx_bolt->serial);
    tx_bolt->serial = NULL;
    reset_stroke(tx_bolt);
    tx_bolt->queued_stroke_count = 0;
}

const char *tx_bolt_port_path(const Tx_Bolt *tx_bolt)
{
    return tx_bolt == NULL ? "" : platform_serial_port_path(tx_bolt->serial);
}

bool tx_bolt_had_error(const Tx_Bolt *tx_bolt)
{
    return tx_bolt != NULL && (tx_bolt->had_error || platform_serial_had_error(tx_bolt->serial));
}

bool tx_bolt_decode_byte(Tx_Bolt *tx_bolt, uint8_t byte, uint64_t *out_bits)
{
    if (tx_bolt == NULL || out_bits == NULL) {
        return false;
    }

    const int key_set = byte >> 6;
    if (key_set <= tx_bolt->last_key_set && !finish_current_stroke(tx_bolt)) {
        return false;
    }

    tx_bolt->last_key_set = key_set;
    const int bit_count = key_set == 3 ? 5 : 6;
    for (int bit_index = 0; bit_index < bit_count; ++bit_index) {
        if (((byte >> bit_index) & 1) != 0) {
            tx_bolt->stroke_bits |= TX_BOLT_BITS[key_set][bit_index];
        }
    }

    if (key_set == 3 && !finish_current_stroke(tx_bolt)) {
        return false;
    }

    return dequeue_stroke(tx_bolt, out_bits);
}

bool tx_bolt_flush_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits)
{
    if (tx_bolt == NULL || out_bits == NULL) {
        return false;
    }
    if (tx_bolt->queued_stroke_count == 0 && !finish_current_stroke(tx_bolt)) {
        return false;
    }
    return dequeue_stroke(tx_bolt, out_bits);
}

bool tx_bolt_has_partial_stroke(const Tx_Bolt *tx_bolt)
{
    return tx_bolt != NULL && tx_bolt->stroke_bits != 0;
}

bool tx_bolt_read_stroke_nonblocking(Tx_Bolt *tx_bolt, uint64_t *out_bits, bool *out_read_byte)
{
    if (out_read_byte != NULL) {
        *out_read_byte = false;
    }
    if (tx_bolt == NULL || out_bits == NULL || tx_bolt->serial == NULL) {
        return false;
    }

    if (dequeue_stroke(tx_bolt, out_bits)) {
        return true;
    }

    while (!tx_bolt_had_error(tx_bolt)) {
        uint8_t byte = 0;
        const Platform_Serial_Read_Result read_result =
            platform_serial_read_byte(tx_bolt->serial, &byte, 0);
        if (read_result == PLATFORM_SERIAL_READ_ERROR) {
            return false;
        }
        if (read_result == PLATFORM_SERIAL_READ_NONE) {
            return false;
        }

        if (out_read_byte != NULL) {
            *out_read_byte = true;
        }
        if (tx_bolt_decode_byte(tx_bolt, byte, out_bits)) {
            return true;
        }
    }

    return false;
}

bool tx_bolt_read_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits)
{
    if (tx_bolt == NULL || out_bits == NULL || tx_bolt->serial == NULL) {
        return false;
    }

    if (dequeue_stroke(tx_bolt, out_bits)) {
        return true;
    }

    while (!tx_bolt_had_error(tx_bolt)) {
        uint8_t byte = 0;
        const Platform_Serial_Read_Result read_result =
            platform_serial_read_byte(tx_bolt->serial, &byte, 100);
        if (read_result == PLATFORM_SERIAL_READ_ERROR) {
            return false;
        }
        if (read_result == PLATFORM_SERIAL_READ_NONE) {
            return tx_bolt_flush_stroke(tx_bolt, out_bits);
        }
        if (tx_bolt_decode_byte(tx_bolt, byte, out_bits)) {
            return true;
        }
    }

    return false;
}
