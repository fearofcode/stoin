#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

struct Platform_Serial_Port {
    int fd;
    char port_path[PLATFORM_SERIAL_PATH_MAX];
    bool had_error;
};

static bool serial_path_already_listed(
    char out_paths[][PLATFORM_SERIAL_PATH_MAX],
    size_t path_count,
    const char *path
)
{
    for (size_t i = 0; i < path_count; ++i) {
        if (strcmp(out_paths[i], path) == 0) {
            return true;
        }
    }
    return false;
}

static bool add_serial_path(
    char out_paths[][PLATFORM_SERIAL_PATH_MAX],
    size_t max_paths,
    size_t *path_count,
    const char *path
)
{
    if (out_paths == NULL || path_count == NULL || path == NULL || *path_count >= max_paths) {
        return false;
    }
    if (serial_path_already_listed(out_paths, *path_count, path)) {
        return false;
    }

    const int written = snprintf(out_paths[*path_count], PLATFORM_SERIAL_PATH_MAX, "%s", path);
    if (written <= 0 || (size_t)written >= PLATFORM_SERIAL_PATH_MAX) {
        return false;
    }

    ++*path_count;
    return true;
}

static void add_glob_matches(
    const char *pattern,
    char out_paths[][PLATFORM_SERIAL_PATH_MAX],
    size_t max_paths,
    size_t *path_count
)
{
    if (pattern == NULL || out_paths == NULL || path_count == NULL || *path_count >= max_paths) {
        return;
    }

    glob_t matches = {0};
    const int glob_result = glob(pattern, 0, NULL, &matches);
    if (glob_result != 0 || matches.gl_pathc == 0) {
        globfree(&matches);
        return;
    }

    for (size_t i = 0; i < matches.gl_pathc && *path_count < max_paths; ++i) {
        (void)add_serial_path(out_paths, max_paths, path_count, matches.gl_pathv[i]);
    }
    globfree(&matches);
}

size_t platform_find_serial_devices(char out_paths[][PLATFORM_SERIAL_PATH_MAX], size_t max_paths)
{
    const char *patterns[] = {
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.wchusbserial*",
        "/dev/cu.KeySerial*",
    };

    if (out_paths == NULL || max_paths == 0) {
        return 0;
    }

    size_t path_count = 0;
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        add_glob_matches(patterns[i], out_paths, max_paths, &path_count);
    }
    return path_count;
}

bool platform_find_serial_device(char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0) {
        return false;
    }

    char paths[1][PLATFORM_SERIAL_PATH_MAX] = {{0}};
    if (platform_find_serial_devices(paths, 1) == 0) {
        return false;
    }

    const int written = snprintf(out_path, out_size, "%s", paths[0]);
    return written > 0 && (size_t)written < out_size;
}

bool platform_find_gemini_pr_device(char *out_path, size_t out_size)
{
    return platform_find_serial_device(out_path, out_size);
}

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

bool platform_serial_open(Platform_Serial_Port **out_port, const char *port_path, int baud_rate)
{
    if (out_port == NULL) {
        return false;
    }
    *out_port = NULL;

    if (port_path == NULL) {
        errno = ENODEV;
        return false;
    }

    char copied_path[PLATFORM_SERIAL_PATH_MAX] = {0};
    const int written = snprintf(copied_path, sizeof(copied_path), "%s", port_path);
    if (written <= 0 || (size_t)written >= sizeof(copied_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    const int fd = open(copied_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    if (!configure_serial_port(fd, baud_rate)) {
        close(fd);
        return false;
    }

    Platform_Serial_Port *port = calloc(1, sizeof(*port));
    if (port == NULL) {
        close(fd);
        errno = ENOMEM;
        return false;
    }

    port->fd = fd;
    snprintf(port->port_path, sizeof(port->port_path), "%s", copied_path);
    *out_port = port;
    return true;
}

void platform_serial_close(Platform_Serial_Port *port)
{
    if (port == NULL) {
        return;
    }
    if (port->fd >= 0) {
        close(port->fd);
        port->fd = -1;
    }
    free(port);
}

const char *platform_serial_port_path(const Platform_Serial_Port *port)
{
    return port == NULL ? "" : port->port_path;
}

bool platform_serial_had_error(const Platform_Serial_Port *port)
{
    return port != NULL && port->had_error;
}

Platform_Serial_Read_Result platform_serial_read_byte(
    Platform_Serial_Port *port,
    uint8_t *out_byte,
    unsigned int timeout_ms
)
{
    if (port == NULL || out_byte == NULL || port->fd < 0) {
        if (port != NULL) {
            port->had_error = true;
        }
        errno = EBADF;
        return PLATFORM_SERIAL_READ_ERROR;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(port->fd, &read_fds);

    struct timeval timeout = {
        .tv_sec = (time_t)(timeout_ms / 1000),
        .tv_usec = (suseconds_t)((timeout_ms % 1000) * 1000),
    };

    const int ready = select(port->fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
        if (errno == EINTR) {
            return PLATFORM_SERIAL_READ_NONE;
        }
        port->had_error = true;
        return PLATFORM_SERIAL_READ_ERROR;
    }
    if (ready == 0) {
        return PLATFORM_SERIAL_READ_NONE;
    }

    const ssize_t bytes_read = read(port->fd, out_byte, 1);
    if (bytes_read == 1) {
        return PLATFORM_SERIAL_READ_BYTE;
    }
    if (bytes_read == 0 || errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return PLATFORM_SERIAL_READ_NONE;
    }

    port->had_error = true;
    return PLATFORM_SERIAL_READ_ERROR;
}
