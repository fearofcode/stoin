#include "platform_linux_internal.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "../stb_ds.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define BIT_WORD(bit) ((bit) / BITS_PER_LONG)
#define BIT_MASK(bit) (1UL << ((bit) % BITS_PER_LONG))
#define BIT_ARRAY_LENGTH(max_bit) (((max_bit) / BITS_PER_LONG) + 1)

typedef struct Linux_Key_Map {
    const char *name;
    uint16_t logical_keycode;
    unsigned int evdev_keycode;
    char printable;
} Linux_Key_Map;

Linux_State g_linux = {
    .uinput_fd = -1,
    .file_watcher_fd = -1,
};

static const Linux_Key_Map LINUX_KEY_MAP[] = {
    {"a", 0, KEY_A, 'a'},
    {"s", 1, KEY_S, 's'},
    {"d", 2, KEY_D, 'd'},
    {"f", 3, KEY_F, 'f'},
    {"h", 4, KEY_H, 'h'},
    {"g", 5, KEY_G, 'g'},
    {"z", 6, KEY_Z, 'z'},
    {"x", 7, KEY_X, 'x'},
    {"c", 8, KEY_C, 'c'},
    {"v", 9, KEY_V, 'v'},
    {"b", 11, KEY_B, 'b'},
    {"q", 12, KEY_Q, 'q'},
    {"w", 13, KEY_W, 'w'},
    {"e", 14, KEY_E, 'e'},
    {"r", 15, KEY_R, 'r'},
    {"y", 16, KEY_Y, 'y'},
    {"t", 17, KEY_T, 't'},
    {"1", 18, KEY_1, '1'},
    {"2", 19, KEY_2, '2'},
    {"3", 20, KEY_3, '3'},
    {"4", 21, KEY_4, '4'},
    {"6", 22, KEY_6, '6'},
    {"5", 23, KEY_5, '5'},
    {"9", 25, KEY_9, '9'},
    {"7", 26, KEY_7, '7'},
    {"8", 28, KEY_8, '8'},
    {"0", 29, KEY_0, '0'},
    {"]", 30, KEY_RIGHTBRACE, ']'},
    {"o", 31, KEY_O, 'o'},
    {"u", 32, KEY_U, 'u'},
    {"[", 33, KEY_LEFTBRACE, '['},
    {"i", 34, KEY_I, 'i'},
    {"p", 35, KEY_P, 'p'},
    {"l", 37, KEY_L, 'l'},
    {"j", 38, KEY_J, 'j'},
    {"'", 39, KEY_APOSTROPHE, '\''},
    {"apostrophe", 39, KEY_APOSTROPHE, '\''},
    {"quote", 39, KEY_APOSTROPHE, '\''},
    {"k", 40, KEY_K, 'k'},
    {";", 41, KEY_SEMICOLON, ';'},
    {"semicolon", 41, KEY_SEMICOLON, ';'},
    {"\\", 42, KEY_BACKSLASH, '\\'},
    {"backslash", 42, KEY_BACKSLASH, '\\'},
    {",", 43, KEY_COMMA, ','},
    {"comma", 43, KEY_COMMA, ','},
    {"/", 44, KEY_SLASH, '/'},
    {"slash", 44, KEY_SLASH, '/'},
    {"n", 45, KEY_N, 'n'},
    {"m", 46, KEY_M, 'm'},
    {".", 47, KEY_DOT, '.'},
    {"period", 47, KEY_DOT, '.'},
    {"dot", 47, KEY_DOT, '.'},
    {"tab", 48, KEY_TAB, '\0'},
    {"space", 49, KEY_SPACE, ' '},
    {"`", 50, KEY_GRAVE, '`'},
    {"backspace", 51, KEY_BACKSPACE, '\0'},
    {"enter", 36, KEY_ENTER, '\0'},
    {"return", 36, KEY_ENTER, '\0'},
    {"escape", 53, KEY_ESC, '\0'},
    {"esc", 53, KEY_ESC, '\0'},
    {"right_command", 54, KEY_RIGHTMETA, '\0'},
    {"right_super", 54, KEY_RIGHTMETA, '\0'},
    {"left_command", 55, KEY_LEFTMETA, '\0'},
    {"left_super", 55, KEY_LEFTMETA, '\0'},
    {"left_shift", 56, KEY_LEFTSHIFT, '\0'},
    {"left_option", 58, KEY_LEFTALT, '\0'},
    {"left_alt", 58, KEY_LEFTALT, '\0'},
    {"left_control", 59, KEY_LEFTCTRL, '\0'},
    {"left_ctrl", 59, KEY_LEFTCTRL, '\0'},
    {"right_shift", 60, KEY_RIGHTSHIFT, '\0'},
    {"right_option", 61, KEY_RIGHTALT, '\0'},
    {"right_alt", 61, KEY_RIGHTALT, '\0'},
    {"right_control", 62, KEY_RIGHTCTRL, '\0'},
    {"right_ctrl", 62, KEY_RIGHTCTRL, '\0'},
};

static bool test_bit(unsigned int bit, const unsigned long *array, size_t array_length)
{
    return BIT_WORD(bit) < array_length && (array[BIT_WORD(bit)] & BIT_MASK(bit)) != 0;
}

static char *copy_cstring(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    const size_t length = strlen(s);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, length + 1);
    return copy;
}

static bool key_name_equals(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool platform_keycode_from_name(const char *name, uint16_t *out_keycode)
{
    if (name == NULL || out_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (key_name_equals(name, LINUX_KEY_MAP[i].name)) {
            *out_keycode = LINUX_KEY_MAP[i].logical_keycode;
            return true;
        }
    }

    return false;
}

bool linux_evdev_keycode_from_logical(uint16_t logical_keycode, unsigned int *out_evdev_keycode)
{
    if (out_evdev_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].logical_keycode == logical_keycode) {
            *out_evdev_keycode = LINUX_KEY_MAP[i].evdev_keycode;
            return true;
        }
    }
    return false;
}

bool linux_logical_keycode_from_evdev(unsigned int evdev_keycode, uint16_t *out_logical_keycode)
{
    if (out_logical_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].evdev_keycode == evdev_keycode) {
            *out_logical_keycode = LINUX_KEY_MAP[i].logical_keycode;
            return true;
        }
    }
    return false;
}

char linux_printable_from_logical(uint16_t logical_keycode)
{
    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].logical_keycode == logical_keycode && LINUX_KEY_MAP[i].printable != '\0') {
            return LINUX_KEY_MAP[i].printable;
        }
    }
    return '\0';
}

static void update_modifier_state(uint16_t logical_keycode, bool is_down)
{
    switch (logical_keycode) {
    case 54:
    case 55:
        g_linux.command_down = is_down;
        break;
    case 56:
    case 60:
        g_linux.shift_down = is_down;
        break;
    case 58:
    case 61:
        g_linux.option_down = is_down;
        break;
    case 59:
    case 62:
        g_linux.control_down = is_down;
        break;
    default:
        break;
    }
}

static bool device_name_is_stoin_virtual_keyboard(const char *name)
{
    return name != NULL && strcmp(name, STOIN_LINUX_UINPUT_NAME) == 0;
}

static bool input_device_is_keyboard(int fd)
{
    unsigned long ev_bits[BIT_ARRAY_LENGTH(EV_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        return false;
    }
    if (!test_bit(EV_KEY, ev_bits, sizeof(ev_bits) / sizeof(ev_bits[0]))) {
        return false;
    }

    unsigned long key_bits[BIT_ARRAY_LENGTH(KEY_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    return test_bit(KEY_A, key_bits, sizeof(key_bits) / sizeof(key_bits[0]))
        && test_bit(KEY_SPACE, key_bits, sizeof(key_bits) / sizeof(key_bits[0]))
        && test_bit(KEY_ENTER, key_bits, sizeof(key_bits) / sizeof(key_bits[0]));
}

typedef enum Linux_Keyboard_Open_Result {
    LINUX_KEYBOARD_OPEN_ADDED,
    LINUX_KEYBOARD_OPEN_PERMISSION_DENIED,
    LINUX_KEYBOARD_OPEN_FAILED,
    LINUX_KEYBOARD_OPEN_NOT_KEYBOARD,
    LINUX_KEYBOARD_OPEN_GRAB_FAILED,
    LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED,
} Linux_Keyboard_Open_Result;

typedef struct Linux_Keyboard_Open_Report {
    size_t path_count;
    size_t added_count;
    size_t permission_denied_count;
    size_t open_failed_count;
    size_t non_keyboard_count;
    size_t grab_failed_count;
    size_t allocation_failed_count;
} Linux_Keyboard_Open_Report;

static Linux_Keyboard_Open_Result add_keyboard_device(const char *path)
{
    if (path == NULL) {
        return LINUX_KEYBOARD_OPEN_FAILED;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return (errno == EACCES || errno == EPERM)
            ? LINUX_KEYBOARD_OPEN_PERMISSION_DENIED
            : LINUX_KEYBOARD_OPEN_FAILED;
    }

    char name[256] = {0};
    (void)ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    if (device_name_is_stoin_virtual_keyboard(name) || !input_device_is_keyboard(fd)) {
        close(fd);
        return LINUX_KEYBOARD_OPEN_NOT_KEYBOARD;
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        fprintf(stderr, "stoin: warning: failed to grab Linux keyboard device %s", path);
        if (errno != 0) {
            fprintf(stderr, " (%s)", strerror(errno));
        }
        fputc('\n', stderr);
        close(fd);
        return LINUX_KEYBOARD_OPEN_GRAB_FAILED;
    }

    char *path_copy = copy_cstring(path);
    if (path_copy == NULL) {
        (void)ioctl(fd, EVIOCGRAB, 0);
        close(fd);
        return LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED;
    }

    Linux_Keyboard_Device device = {
        .fd = fd,
        .path = path_copy,
    };
    snprintf(device.name, sizeof(device.name), "%s", name[0] != '\0' ? name : path);
    arrput(g_linux.keyboards, device);
    return LINUX_KEYBOARD_OPEN_ADDED;
}

static void close_keyboard_devices(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
        if (g_linux.keyboards[i].fd >= 0) {
            (void)ioctl(g_linux.keyboards[i].fd, EVIOCGRAB, 0);
            close(g_linux.keyboards[i].fd);
        }
        free(g_linux.keyboards[i].path);
    }
    arrfree(g_linux.keyboards);
    g_linux.keyboards = NULL;
}

static void print_keyboard_open_report(const Linux_Keyboard_Open_Report *report)
{
    if (report == NULL) {
        return;
    }

    fprintf(stderr,
        "stoin: checked %zu Linux input event devices: %zu opened, %zu permission denied, %zu open failed, %zu not keyboards, %zu grab failed\n",
        report->path_count,
        report->added_count,
        report->permission_denied_count,
        report->open_failed_count,
        report->non_keyboard_count,
        report->grab_failed_count);
    if (report->permission_denied_count > 0) {
        fputs("stoin: qwerty capture needs read/grab access to keyboard devices under /dev/input/event*\n", stderr);
        fputs("stoin: see docs/linux-setup.md for the stoin input-device udev rule\n", stderr);
    }
}

static bool open_keyboard_devices(void)
{
    glob_t matches = {0};
    const int glob_result = glob("/dev/input/event*", 0, NULL, &matches);
    if (glob_result != 0 || matches.gl_pathc == 0) {
        globfree(&matches);
        fputs("stoin: no Linux input event devices found under /dev/input/event*\n", stderr);
        return false;
    }

    Linux_Keyboard_Open_Report report = {
        .path_count = matches.gl_pathc,
    };

    for (size_t i = 0; i < matches.gl_pathc; ++i) {
        switch (add_keyboard_device(matches.gl_pathv[i])) {
        case LINUX_KEYBOARD_OPEN_ADDED:
            ++report.added_count;
            break;
        case LINUX_KEYBOARD_OPEN_PERMISSION_DENIED:
            ++report.permission_denied_count;
            break;
        case LINUX_KEYBOARD_OPEN_FAILED:
            ++report.open_failed_count;
            break;
        case LINUX_KEYBOARD_OPEN_NOT_KEYBOARD:
            ++report.non_keyboard_count;
            break;
        case LINUX_KEYBOARD_OPEN_GRAB_FAILED:
            ++report.grab_failed_count;
            break;
        case LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED:
            ++report.allocation_failed_count;
            break;
        }
    }

    globfree(&matches);
    if (arrlenu(g_linux.keyboards) == 0) {
        print_keyboard_open_report(&report);
    }
    return arrlenu(g_linux.keyboards) > 0;
}

static void process_key_event(const struct input_event *event)
{
    if (event == NULL || event->type != EV_KEY) {
        return;
    }

    uint16_t logical_keycode = 0;
    const bool mapped = linux_logical_keycode_from_evdev(event->code, &logical_keycode);
    const bool is_down = event->value != 0;
    const bool is_repeat = event->value == 2;

    if (mapped && !is_repeat) {
        update_modifier_state(logical_keycode, is_down);
    }

    bool consumed = false;
    if (mapped && g_linux.handler != NULL) {
        const Input_Event input = {
            .keycode = logical_keycode,
            .is_down = is_down,
            .is_repeat = is_repeat,
            .shift = g_linux.shift_down,
            .control = g_linux.control_down,
            .option = g_linux.option_down,
            .command = g_linux.command_down,
            .printable = linux_printable_from_logical(logical_keycode),
        };
        consumed = g_linux.handler(&input, g_linux.userdata);
    }

    if (!consumed) {
        (void)linux_uinput_emit_key_event(event->code, event->value);
    }
}

static void process_keyboard_device(Linux_Keyboard_Device *device)
{
    if (device == NULL || device->fd < 0) {
        return;
    }

    while (true) {
        struct input_event event = {0};
        const ssize_t bytes_read = read(device->fd, &event, sizeof(event));
        if (bytes_read == (ssize_t)sizeof(event)) {
            process_key_event(&event);
            continue;
        }
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

bool platform_user_session_is_active(void)
{
    return true;
}

void platform_translation_timing_set_enabled(bool enabled)
{
    g_linux.translation_timing_enabled = enabled;
    if (!enabled) {
        g_linux.translation_timing_active = false;
        g_linux.translation_timing_start_ns = 0;
    }
}

void platform_translation_timing_begin(uint64_t start_ns)
{
    if (!g_linux.translation_timing_enabled || start_ns == 0) {
        return;
    }

    g_linux.translation_timing_start_ns = start_ns;
    g_linux.translation_timing_active = true;
}

void platform_translation_timing_cancel(void)
{
    g_linux.translation_timing_active = false;
    g_linux.translation_timing_start_ns = 0;
}

void linux_report_translation_timing_before_output(const char *operation)
{
    if (!g_linux.translation_timing_enabled || !g_linux.translation_timing_active) {
        return;
    }

    const uint64_t now_ns = platform_monotonic_ns();
    const uint64_t start_ns = g_linux.translation_timing_start_ns;
    const uint64_t elapsed_ns = now_ns >= start_ns ? now_ns - start_ns : 0;
    g_linux.translation_timing_active = false;
    g_linux.translation_timing_start_ns = 0;
    ++g_linux.translation_timing_sequence;

    fprintf(stderr,
        "stoin: translation latency #%" PRIu64 " before %s uinput event: %.3f ms (%.1f us)\n",
        g_linux.translation_timing_sequence,
        operation != NULL ? operation : "first",
        (double)elapsed_ns / 1000000.0,
        (double)elapsed_ns / 1000.0);
}

uint64_t platform_monotonic_ns(void)
{
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

uint64_t platform_monotonic_ms(void)
{
    return platform_monotonic_ns() / UINT64_C(1000000);
}

void platform_sleep_ms(unsigned int milliseconds)
{
    struct timespec requested = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };

    while (nanosleep(&requested, &requested) != 0 && errno == EINTR) {
    }
}

bool platform_init(Handle_Input_Fn handler, void *userdata)
{
    g_linux.handler = handler;
    g_linux.userdata = userdata;

    if (!platform_output_init()) {
        return false;
    }

    if (!open_keyboard_devices()) {
        fputs("stoin: failed to open and grab any Linux keyboard devices under /dev/input/event*\n", stderr);
        fputs("stoin: check qwerty capture permissions for /dev/input/event*, then restart stoin\n", stderr);
        platform_shutdown();
        return false;
    }

    g_linux.running = true;
    return true;
}

void platform_run(void)
{
    g_linux.running = true;
    while (g_linux.running) {
        struct pollfd *fds = NULL;

        for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
            if (g_linux.keyboards[i].fd >= 0) {
                arrput(fds, ((struct pollfd) {
                    .fd = g_linux.keyboards[i].fd,
                    .events = POLLIN,
                }));
            }
        }

        const int watcher_fd = linux_file_watcher_fd();
        const size_t watcher_index = arrlenu(fds);
        if (watcher_fd >= 0) {
            arrput(fds, ((struct pollfd) {
                .fd = watcher_fd,
                .events = POLLIN,
            }));
        }

        if (arrlenu(fds) == 0) {
            arrfree(fds);
            platform_sleep_ms(250);
            continue;
        }

        const int ready = poll(fds, (nfds_t)arrlenu(fds), -1);
        if (ready < 0) {
            arrfree(fds);
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        size_t fd_index = 0;
        for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
            if (g_linux.keyboards[i].fd < 0) {
                continue;
            }
            if ((fds[fd_index].revents & POLLIN) != 0) {
                process_keyboard_device(&g_linux.keyboards[i]);
            }
            ++fd_index;
        }

        if (watcher_fd >= 0
            && watcher_index < arrlenu(fds)
            && (fds[watcher_index].revents & POLLIN) != 0) {
            platform_file_watcher_poll();
        }

        arrfree(fds);
    }
}

void platform_shutdown(void)
{
    g_linux.running = false;
    platform_file_watcher_stop();
    platform_pedals_shutdown();
    close_keyboard_devices();
    linux_output_shutdown();
    g_linux.handler = NULL;
    g_linux.userdata = NULL;
    g_linux.shift_down = false;
    g_linux.control_down = false;
    g_linux.option_down = false;
    g_linux.command_down = false;
}

bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn handler,
    void *userdata
)
{
    (void)config_path;
    (void)handler;
    (void)userdata;

    if (register_role != PLATFORM_PEDAL_ROLE_NONE) {
        fputs("stoin: USB pedal registration is not implemented on Linux yet\n", stderr);
        return false;
    }
    return true;
}

void platform_pedals_poll(void)
{
}

void platform_pedals_shutdown(void)
{
}
