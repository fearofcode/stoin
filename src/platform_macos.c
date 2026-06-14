#include "platform.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../stb_ds.h"

#define STOIN_GENERATED_EVENT_USER_DATA 0x73746f696eULL
#define MAC_BACKSPACE_KEYCODE 51
#define MAC_SESSION_SCREEN_IS_LOCKED_KEY CFSTR("CGSSessionScreenIsLocked")
#define MAC_SESSION_AGENT_SCREEN_IS_LOCKED "com.apple.sessionagent.screenIsLocked"

typedef struct Mac_State {
    CFMachPortRef tap;
    CFRunLoopSourceRef run_loop_source;
    CFRunLoopRef run_loop;
    CGEventSourceRef output_source;
    Handle_Input_Fn handler;
    void *userdata;
    int screen_lock_notify_token;
    bool screen_lock_notify_registered;
} Mac_State;

struct Platform_Serial_Port {
    int fd;
    char port_path[256];
    bool had_error;
};

static Mac_State g_macos;

static char keycode_to_us_qwerty_printable(CGKeyCode keycode)
{
    switch (keycode) {
    case 0: return 'a';
    case 11: return 'b';
    case 8: return 'c';
    case 2: return 'd';
    case 14: return 'e';
    case 3: return 'f';
    case 5: return 'g';
    case 4: return 'h';
    case 34: return 'i';
    case 38: return 'j';
    case 40: return 'k';
    case 37: return 'l';
    case 46: return 'm';
    case 45: return 'n';
    case 31: return 'o';
    case 35: return 'p';
    case 12: return 'q';
    case 15: return 'r';
    case 1: return 's';
    case 17: return 't';
    case 32: return 'u';
    case 9: return 'v';
    case 13: return 'w';
    case 7: return 'x';
    case 16: return 'y';
    case 6: return 'z';
    case 18: return '1';
    case 19: return '2';
    case 20: return '3';
    case 21: return '4';
    case 23: return '5';
    case 22: return '6';
    case 26: return '7';
    case 28: return '8';
    case 25: return '9';
    case 29: return '0';
    case 50: return '`';
    case 27: return '-';
    case 24: return '=';
    case 33: return '[';
    case 30: return ']';
    case 42: return '\\';
    case 41: return ';';
    case 39: return '\'';
    case 43: return ',';
    case 47: return '.';
    case 44: return '/';
    case 49: return ' ';
    default: return '\0';
    }
}

bool platform_keycode_from_name(const char *name, uint16_t *out_keycode)
{
    if (name == NULL || out_keycode == NULL) {
        return false;
    }

    if (strlen(name) == 1) {
        switch (name[0]) {
        case 'a': *out_keycode = 0; return true;
        case 's': *out_keycode = 1; return true;
        case 'd': *out_keycode = 2; return true;
        case 'f': *out_keycode = 3; return true;
        case 'h': *out_keycode = 4; return true;
        case 'g': *out_keycode = 5; return true;
        case 'z': *out_keycode = 6; return true;
        case 'x': *out_keycode = 7; return true;
        case 'c': *out_keycode = 8; return true;
        case 'v': *out_keycode = 9; return true;
        case 'b': *out_keycode = 11; return true;
        case 'q': *out_keycode = 12; return true;
        case 'w': *out_keycode = 13; return true;
        case 'e': *out_keycode = 14; return true;
        case 'r': *out_keycode = 15; return true;
        case 'y': *out_keycode = 16; return true;
        case 't': *out_keycode = 17; return true;
        case '1': *out_keycode = 18; return true;
        case '2': *out_keycode = 19; return true;
        case '3': *out_keycode = 20; return true;
        case '4': *out_keycode = 21; return true;
        case '6': *out_keycode = 22; return true;
        case '5': *out_keycode = 23; return true;
        case '9': *out_keycode = 25; return true;
        case '7': *out_keycode = 26; return true;
        case '8': *out_keycode = 28; return true;
        case '0': *out_keycode = 29; return true;
        case ']': *out_keycode = 30; return true;
        case 'o': *out_keycode = 31; return true;
        case 'u': *out_keycode = 32; return true;
        case '[': *out_keycode = 33; return true;
        case 'i': *out_keycode = 34; return true;
        case 'p': *out_keycode = 35; return true;
        case 'l': *out_keycode = 37; return true;
        case 'j': *out_keycode = 38; return true;
        case '\'': *out_keycode = 39; return true;
        case 'k': *out_keycode = 40; return true;
        case ';': *out_keycode = 41; return true;
        case '\\': *out_keycode = 42; return true;
        case ',': *out_keycode = 43; return true;
        case '/': *out_keycode = 44; return true;
        case 'n': *out_keycode = 45; return true;
        case 'm': *out_keycode = 46; return true;
        case '.': *out_keycode = 47; return true;
        case '`': *out_keycode = 50; return true;
        default: return false;
        }
    }

    if (strcmp(name, "space") == 0) {
        *out_keycode = 49;
    } else if (strcmp(name, "tab") == 0) {
        *out_keycode = 48;
    } else if (strcmp(name, "enter") == 0 || strcmp(name, "return") == 0) {
        *out_keycode = 36;
    } else if (strcmp(name, "escape") == 0 || strcmp(name, "esc") == 0) {
        *out_keycode = 53;
    } else if (strcmp(name, "backspace") == 0) {
        *out_keycode = 51;
    } else if (strcmp(name, "semicolon") == 0) {
        *out_keycode = 41;
    } else if (strcmp(name, "apostrophe") == 0 || strcmp(name, "quote") == 0) {
        *out_keycode = 39;
    } else if (strcmp(name, "comma") == 0) {
        *out_keycode = 43;
    } else if (strcmp(name, "period") == 0 || strcmp(name, "dot") == 0) {
        *out_keycode = 47;
    } else if (strcmp(name, "slash") == 0) {
        *out_keycode = 44;
    } else if (strcmp(name, "backslash") == 0) {
        *out_keycode = 42;
    } else if (strcmp(name, "right_shift") == 0) {
        *out_keycode = 60;
    } else if (strcmp(name, "left_shift") == 0) {
        *out_keycode = 56;
    } else {
        return false;
    }

    return true;
}

static bool find_first_glob_match(const char *pattern, char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0) {
        return false;
    }

    glob_t matches = {0};
    const int glob_result = glob(pattern, 0, NULL, &matches);
    if (glob_result != 0 || matches.gl_pathc == 0) {
        globfree(&matches);
        return false;
    }

    const int written = snprintf(out_path, out_size, "%s", matches.gl_pathv[0]);
    globfree(&matches);
    return written > 0 && (size_t)written < out_size;
}

bool platform_find_serial_device(char *out_path, size_t out_size)
{
    const char *patterns[] = {
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.wchusbserial*",
        "/dev/cu.KeySerial*",
    };

    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        if (find_first_glob_match(patterns[i], out_path, out_size)) {
            return true;
        }
    }
    return false;
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

    char copied_path[256] = {0};
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

static bool cf_dictionary_bool_value(CFDictionaryRef dictionary, CFStringRef key, bool default_value)
{
    if (dictionary == NULL || key == NULL) {
        return default_value;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (value == NULL || CFGetTypeID(value) != CFBooleanGetTypeID()) {
        return default_value;
    }
    return CFBooleanGetValue(value);
}

static bool screen_is_locked_by_notify_state(void)
{
    if (!g_macos.screen_lock_notify_registered) {
        int token = NOTIFY_TOKEN_INVALID;
        if (notify_register_check(MAC_SESSION_AGENT_SCREEN_IS_LOCKED, &token) != NOTIFY_STATUS_OK) {
            return false;
        }
        g_macos.screen_lock_notify_token = token;
        g_macos.screen_lock_notify_registered = true;
    }

    uint64_t state = 0;
    if (notify_get_state(g_macos.screen_lock_notify_token, &state) != NOTIFY_STATUS_OK) {
        return false;
    }
    return state != 0;
}

bool platform_user_session_is_active(void)
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == NULL) {
        return false;
    }

    const bool on_console = cf_dictionary_bool_value(session, kCGSessionOnConsoleKey, false);
    const bool login_done = cf_dictionary_bool_value(session, kCGSessionLoginDoneKey, false);
    const bool screen_locked =
        cf_dictionary_bool_value(session, MAC_SESSION_SCREEN_IS_LOCKED_KEY, false)
        || screen_is_locked_by_notify_state();
    CFRelease(session);

    return on_console && login_done && !screen_locked;
}

bool platform_file_stamp(const char *path, Platform_File_Stamp *out_stamp)
{
    if (path == NULL || out_stamp == NULL) {
        return false;
    }

    memset(out_stamp, 0, sizeof(*out_stamp));

    struct stat info;
    if (stat(path, &info) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        return false;
    }

    out_stamp->exists = true;
    out_stamp->size = (uint64_t)info.st_size;
    out_stamp->modified_time_ns = (uint64_t)info.st_mtimespec.tv_sec * UINT64_C(1000000000)
        + (uint64_t)info.st_mtimespec.tv_nsec;
    return true;
}

void platform_sleep_ms(unsigned int milliseconds)
{
    struct timespec requested = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };

    while (nanosleep(&requested, &requested) != 0) {
    }
}

static bool check_accessibility_trust(void)
{
    const void *keys[] = { kAXTrustedCheckOptionPrompt };
    const void *values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    const Boolean trusted = AXIsProcessTrustedWithOptions(options);
    if (options != NULL) {
        CFRelease(options);
    }

    if (!trusted) {
        fputs("stoin: Accessibility permission is required for the keyboard event tap.\n", stderr);
        fputs("stoin: Open System Settings > Privacy & Security > Accessibility, enable this terminal/app, then run stoin again.\n", stderr);
    }

    return trusted;
}

static void mark_generated_event(CGEventRef event)
{
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, STOIN_GENERATED_EVENT_USER_DATA);
}

static bool event_was_generated_by_stoin(CGEventRef event)
{
    return CGEventGetIntegerValueField(event, kCGEventSourceUserData) == STOIN_GENERATED_EVENT_USER_DATA;
}

static CGEventRef keyboard_tap_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *user_info)
{
    (void)proxy;
    (void)user_info;

    if (type == kCGEventTapDisabledByTimeout) {
        if (g_macos.tap != NULL) {
            CGEventTapEnable(g_macos.tap, true);
        }
        fputs("stoin: event tap timed out and was re-enabled; some keystrokes may have been missed\n", stderr);
        return NULL;
    }

    if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
        return event;
    }

    if (event_was_generated_by_stoin(event)) {
        return event;
    }

    const CGEventFlags flags = CGEventGetFlags(event);
    const CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    const int64_t repeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat);
    bool is_down = type == kCGEventKeyDown;

    if (type == kCGEventFlagsChanged) {
        switch (keycode) {
        case 56:
        case 60:
            is_down = (flags & kCGEventFlagMaskShift) != 0;
            break;
        case 58:
        case 61:
            is_down = (flags & kCGEventFlagMaskAlternate) != 0;
            break;
        case 59:
        case 62:
            is_down = (flags & kCGEventFlagMaskControl) != 0;
            break;
        case 55:
        case 54:
            is_down = (flags & kCGEventFlagMaskCommand) != 0;
            break;
        default:
            is_down = false;
            break;
        }
    }

    Input_Event input = {
        .keycode = keycode,
        .is_down = is_down,
        .is_repeat = repeat != 0,
        .shift = (flags & kCGEventFlagMaskShift) != 0,
        .control = (flags & kCGEventFlagMaskControl) != 0,
        .option = (flags & kCGEventFlagMaskAlternate) != 0,
        .command = (flags & kCGEventFlagMaskCommand) != 0,
        .printable = keycode_to_us_qwerty_printable(keycode),
    };

    if (g_macos.handler != NULL && g_macos.handler(&input, g_macos.userdata)) {
        return NULL;
    }

    return event;
}

bool platform_init(Handle_Input_Fn handler, void *userdata)
{
    g_macos.handler = handler;
    g_macos.userdata = userdata;

    if (!platform_output_init()) {
        return false;
    }

    const CGEventMask keyboard_events = CGEventMaskBit(kCGEventKeyDown)
        | CGEventMaskBit(kCGEventKeyUp)
        | CGEventMaskBit(kCGEventFlagsChanged);
    g_macos.tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        keyboard_events,
        keyboard_tap_callback,
        NULL
    );

    if (g_macos.tap == NULL) {
        fputs("stoin: failed to create keyboard event tap\n", stderr);
        fputs("stoin: confirm Accessibility permission, then restart the executable\n", stderr);
        platform_shutdown();
        return false;
    }

    g_macos.run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_macos.tap, 0);
    if (g_macos.run_loop_source == NULL) {
        fputs("stoin: failed to create run loop source for event tap\n", stderr);
        platform_shutdown();
        return false;
    }

    g_macos.run_loop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(g_macos.run_loop, g_macos.run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(g_macos.tap, true);

    return true;
}

bool platform_output_init(void)
{
    if (g_macos.output_source != NULL) {
        return true;
    }

    if (!check_accessibility_trust()) {
        return false;
    }

    g_macos.output_source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (g_macos.output_source == NULL) {
        fputs("stoin: failed to create CoreGraphics event source\n", stderr);
        return false;
    }

    return true;
}

void platform_run(void)
{
    CFRunLoopRun();
}

void platform_shutdown(void)
{
    if (g_macos.tap != NULL) {
        CGEventTapEnable(g_macos.tap, false);
    }

    if (g_macos.run_loop != NULL && g_macos.run_loop_source != NULL) {
        CFRunLoopRemoveSource(g_macos.run_loop, g_macos.run_loop_source, kCFRunLoopCommonModes);
    }

    if (g_macos.run_loop_source != NULL) {
        CFRunLoopSourceInvalidate(g_macos.run_loop_source);
        CFRelease(g_macos.run_loop_source);
        g_macos.run_loop_source = NULL;
    }

    if (g_macos.tap != NULL) {
        CFMachPortInvalidate(g_macos.tap);
        CFRelease(g_macos.tap);
        g_macos.tap = NULL;
    }

    if (g_macos.output_source != NULL) {
        CFRelease(g_macos.output_source);
        g_macos.output_source = NULL;
    }

    if (g_macos.screen_lock_notify_registered) {
        notify_cancel(g_macos.screen_lock_notify_token);
        g_macos.screen_lock_notify_token = 0;
        g_macos.screen_lock_notify_registered = false;
    }

    if (g_macos.run_loop != NULL) {
        CFRunLoopStop(g_macos.run_loop);
        g_macos.run_loop = NULL;
    }

    g_macos.handler = NULL;
    g_macos.userdata = NULL;
}

static bool post_keyboard_event_pair(CGKeyCode keycode)
{
    CGEventRef key_down = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, false);
    bool ok = false;

    if (key_down != NULL && key_up != NULL) {
        mark_generated_event(key_down);
        mark_generated_event(key_up);
        CGEventPost(kCGSessionEventTap, key_down);
        CGEventPost(kCGSessionEventTap, key_up);
        ok = true;
    }

    if (key_down != NULL) {
        CFRelease(key_down);
    }
    if (key_up != NULL) {
        CFRelease(key_up);
    }

    return ok;
}

static bool count_composed_characters(CFStringRef string, size_t *out_count)
{
    if (string == NULL || out_count == NULL) {
        return false;
    }

    size_t count = 0;
    const CFIndex length = CFStringGetLength(string);
    for (CFIndex index = 0; index < length;) {
        const CFRange range = CFStringGetRangeOfComposedCharactersAtIndex(string, index);
        if (range.length <= 0) {
            return false;
        }
        ++count;
        index = range.location + range.length;
    }

    *out_count = count;
    return true;
}

bool platform_send_text_utf8(const char *utf8)
{
    if (utf8 == NULL) {
        return false;
    }
    if (!platform_user_session_is_active()) {
        return false;
    }
    if (!platform_output_init()) {
        return false;
    }

    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, utf8, kCFStringEncodingUTF8);
    if (string == NULL) {
        return false;
    }

    bool ok = false;
    UniChar *utf16 = NULL;
    const CFIndex length = CFStringGetLength(string);
    arrsetlen(utf16, length);

    if (length > 0) {
        CFStringGetCharacters(string, CFRangeMake(0, length), utf16);
    }

    CGEventRef key_down = CGEventCreateKeyboardEvent(g_macos.output_source, 0, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(g_macos.output_source, 0, false);
    if (key_down != NULL && key_up != NULL) {
        mark_generated_event(key_down);
        mark_generated_event(key_up);
        CGEventKeyboardSetUnicodeString(key_down, (UniCharCount)length, utf16);
        CGEventKeyboardSetUnicodeString(key_up, (UniCharCount)length, utf16);
        CGEventPost(kCGSessionEventTap, key_down);
        CGEventPost(kCGSessionEventTap, key_up);
        ok = true;
    }

    if (key_down != NULL) {
        CFRelease(key_down);
    }
    if (key_up != NULL) {
        CFRelease(key_up);
    }

    arrfree(utf16);
    CFRelease(string);
    return ok;
}

bool platform_delete_text_utf8(const char *utf8)
{
    if (utf8 == NULL) {
        return false;
    }
    if (!platform_user_session_is_active()) {
        return false;
    }
    if (!platform_output_init()) {
        return false;
    }

    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, utf8, kCFStringEncodingUTF8);
    if (string == NULL) {
        return false;
    }

    size_t character_count = 0;
    const bool counted = count_composed_characters(string, &character_count);
    CFRelease(string);
    if (!counted) {
        return false;
    }

    for (size_t i = 0; i < character_count; ++i) {
        if (!post_keyboard_event_pair(MAC_BACKSPACE_KEYCODE)) {
            return false;
        }
    }

    return true;
}
