#include "tx_bolt.h"

#include "steno_stroke.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

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

static speed_t baud_rate_to_speed(int baud_rate)
{
    switch (baud_rate) {
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return 0;
    }
}

static bool configure_serial_port(int fd, int baud_rate)
{
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        return false;
    }

    const speed_t speed = baud_rate_to_speed(baud_rate);
    if (speed == 0) {
        errno = EINVAL;
        return false;
    }

    cfmakeraw(&options);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= CS8 | CLOCAL | CREAD;
#ifdef CRTSCTS
    options.c_cflag &= ~CRTSCTS;
#endif
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    if (cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0) {
        return false;
    }
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        return false;
    }

    tcflush(fd, TCIOFLUSH);
    return true;
}

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
    tx_bolt->fd = -1;

    if (config->port_path == NULL) {
        errno = ENODEV;
        return false;
    }

    char port_path[sizeof(tx_bolt->port_path)] = {0};
    const int written = snprintf(port_path, sizeof(port_path), "%s", config->port_path);
    if (written <= 0 || (size_t)written >= sizeof(port_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    const int baud_rate = config->baud_rate == 0 ? TX_BOLT_DEFAULT_BAUD_RATE : config->baud_rate;
    const int fd = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    if (!configure_serial_port(fd, baud_rate)) {
        close(fd);
        return false;
    }

    tx_bolt->fd = fd;
    snprintf(tx_bolt->port_path, sizeof(tx_bolt->port_path), "%s", port_path);
    return true;
}

void tx_bolt_close(Tx_Bolt *tx_bolt)
{
    if (tx_bolt == NULL) {
        return;
    }
    if (tx_bolt->fd >= 0) {
        close(tx_bolt->fd);
        tx_bolt->fd = -1;
    }
    reset_stroke(tx_bolt);
    tx_bolt->queued_stroke_count = 0;
}

const char *tx_bolt_port_path(const Tx_Bolt *tx_bolt)
{
    return tx_bolt == NULL ? "" : tx_bolt->port_path;
}

bool tx_bolt_had_error(const Tx_Bolt *tx_bolt)
{
    return tx_bolt != NULL && tx_bolt->had_error;
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

static ssize_t read_byte(Tx_Bolt *tx_bolt, uint8_t *out_byte)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tx_bolt->fd, &read_fds);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };

    const int ready = select(tx_bolt->fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
        if (errno == EINTR) {
            return 0;
        }
        tx_bolt->had_error = true;
        return -1;
    }
    if (ready == 0) {
        return 0;
    }

    const ssize_t bytes_read = read(tx_bolt->fd, out_byte, 1);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        tx_bolt->had_error = true;
    }
    return bytes_read;
}

bool tx_bolt_read_stroke(Tx_Bolt *tx_bolt, uint64_t *out_bits)
{
    if (tx_bolt == NULL || out_bits == NULL || tx_bolt->fd < 0) {
        return false;
    }

    if (dequeue_stroke(tx_bolt, out_bits)) {
        return true;
    }

    while (!tx_bolt->had_error) {
        uint8_t byte = 0;
        const ssize_t bytes_read = read_byte(tx_bolt, &byte);
        if (bytes_read < 0) {
            return false;
        }
        if (bytes_read == 0) {
            return tx_bolt_flush_stroke(tx_bolt, out_bits);
        }
        if (tx_bolt_decode_byte(tx_bolt, byte, out_bits)) {
            return true;
        }
    }

    return false;
}
