#include "gemini_pr.h"

#include "steno_stroke.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

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

bool gemini_pr_open(Gemini_Pr *gemini, const Gemini_Pr_Config *config)
{
    if (gemini == NULL || config == NULL) {
        return false;
    }

    memset(gemini, 0, sizeof(*gemini));
    gemini->fd = -1;

    if (config->port_path == NULL) {
        errno = ENODEV;
        return false;
    }

    char port_path[sizeof(gemini->port_path)] = {0};
    const int written = snprintf(port_path, sizeof(port_path), "%s", config->port_path);
    if (written <= 0 || (size_t)written >= sizeof(port_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    const int baud_rate = config->baud_rate == 0 ? GEMINI_PR_DEFAULT_BAUD_RATE : config->baud_rate;
    const int fd = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    if (!configure_serial_port(fd, baud_rate)) {
        close(fd);
        return false;
    }

    gemini->fd = fd;
    snprintf(gemini->port_path, sizeof(gemini->port_path), "%s", port_path);
    return true;
}

void gemini_pr_close(Gemini_Pr *gemini)
{
    if (gemini == NULL) {
        return;
    }
    if (gemini->fd >= 0) {
        close(gemini->fd);
        gemini->fd = -1;
    }
    gemini->packet_index = 0;
}

const char *gemini_pr_port_path(const Gemini_Pr *gemini)
{
    return gemini == NULL ? "" : gemini->port_path;
}

bool gemini_pr_had_error(const Gemini_Pr *gemini)
{
    return gemini != NULL && gemini->had_error;
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

static ssize_t read_byte(Gemini_Pr *gemini, uint8_t *out_byte)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(gemini->fd, &read_fds);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };

    const int ready = select(gemini->fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
        if (errno == EINTR) {
            return 0;
        }
        gemini->had_error = true;
        return -1;
    }
    if (ready == 0) {
        return 0;
    }

    const ssize_t bytes_read = read(gemini->fd, out_byte, 1);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        gemini->had_error = true;
    }
    return bytes_read;
}

bool gemini_pr_read_stroke(Gemini_Pr *gemini, uint64_t *out_bits)
{
    if (gemini == NULL || out_bits == NULL || gemini->fd < 0) {
        return false;
    }

    while (!gemini->had_error) {
        uint8_t byte = 0;
        const ssize_t bytes_read = read_byte(gemini, &byte);
        if (bytes_read <= 0) {
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
