#include "raw_serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

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

bool raw_serial_open(Raw_Serial *serial, const Raw_Serial_Config *config)
{
    if (serial == NULL || config == NULL) {
        return false;
    }

    memset(serial, 0, sizeof(*serial));
    serial->fd = -1;

    if (config->port_path == NULL) {
        errno = ENODEV;
        return false;
    }

    char port_path[sizeof(serial->port_path)] = {0};
    const int written = snprintf(port_path, sizeof(port_path), "%s", config->port_path);
    if (written <= 0 || (size_t)written >= sizeof(port_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    const int fd = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    if (!configure_serial_port(fd, config->baud_rate)) {
        close(fd);
        return false;
    }

    serial->fd = fd;
    snprintf(serial->port_path, sizeof(serial->port_path), "%s", port_path);
    return true;
}

void raw_serial_close(Raw_Serial *serial)
{
    if (serial == NULL) {
        return;
    }
    if (serial->fd >= 0) {
        close(serial->fd);
        serial->fd = -1;
    }
}

const char *raw_serial_port_path(const Raw_Serial *serial)
{
    return serial == NULL ? "" : serial->port_path;
}

bool raw_serial_had_error(const Raw_Serial *serial)
{
    return serial != NULL && serial->had_error;
}

Raw_Serial_Read_Result raw_serial_read_byte(Raw_Serial *serial, uint8_t *out_byte)
{
    if (serial == NULL || out_byte == NULL || serial->fd < 0) {
        if (serial != NULL) {
            serial->had_error = true;
        }
        errno = EBADF;
        return RAW_SERIAL_READ_ERROR;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(serial->fd, &read_fds);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };

    const int ready = select(serial->fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
        if (errno == EINTR) {
            return RAW_SERIAL_READ_NONE;
        }
        serial->had_error = true;
        return RAW_SERIAL_READ_ERROR;
    }
    if (ready == 0) {
        return RAW_SERIAL_READ_NONE;
    }

    const ssize_t bytes_read = read(serial->fd, out_byte, 1);
    if (bytes_read == 1) {
        return RAW_SERIAL_READ_BYTE;
    }
    if (bytes_read == 0 || errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return RAW_SERIAL_READ_NONE;
    }

    serial->had_error = true;
    return RAW_SERIAL_READ_ERROR;
}
