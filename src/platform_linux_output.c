#include "platform_linux_internal.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

typedef enum Linux_Combo_Modifier {
    LINUX_COMBO_SHIFT = 1 << 0,
    LINUX_COMBO_CONTROL = 1 << 1,
    LINUX_COMBO_ALT = 1 << 2,
    LINUX_COMBO_SUPER = 1 << 3,
} Linux_Combo_Modifier;

static bool write_uinput_event(unsigned int type, unsigned int code, int value)
{
    if (g_linux.uinput_fd < 0) {
        return false;
    }

    const struct input_event event = {
        .type = (uint16_t)type,
        .code = (uint16_t)code,
        .value = value,
    };
    return write(g_linux.uinput_fd, &event, sizeof(event)) == (ssize_t)sizeof(event);
}

static bool emit_syn_report(void)
{
    return write_uinput_event(EV_SYN, SYN_REPORT, 0);
}

bool linux_uinput_emit_key_event(unsigned int evdev_keycode, int value)
{
    return write_uinput_event(EV_KEY, evdev_keycode, value) && emit_syn_report();
}

bool linux_uinput_tap_key(unsigned int evdev_keycode)
{
    return linux_uinput_emit_key_event(evdev_keycode, 1)
        && linux_uinput_emit_key_event(evdev_keycode, 0);
}

typedef struct Linux_Ascii_Key {
    unsigned int evdev_keycode;
    bool shift;
} Linux_Ascii_Key;

static bool letter_keycode(uint32_t letter, unsigned int *out_keycode)
{
    if (out_keycode == NULL) {
        return false;
    }

    switch (letter) {
    case 'a': *out_keycode = KEY_A; return true;
    case 'b': *out_keycode = KEY_B; return true;
    case 'c': *out_keycode = KEY_C; return true;
    case 'd': *out_keycode = KEY_D; return true;
    case 'e': *out_keycode = KEY_E; return true;
    case 'f': *out_keycode = KEY_F; return true;
    case 'g': *out_keycode = KEY_G; return true;
    case 'h': *out_keycode = KEY_H; return true;
    case 'i': *out_keycode = KEY_I; return true;
    case 'j': *out_keycode = KEY_J; return true;
    case 'k': *out_keycode = KEY_K; return true;
    case 'l': *out_keycode = KEY_L; return true;
    case 'm': *out_keycode = KEY_M; return true;
    case 'n': *out_keycode = KEY_N; return true;
    case 'o': *out_keycode = KEY_O; return true;
    case 'p': *out_keycode = KEY_P; return true;
    case 'q': *out_keycode = KEY_Q; return true;
    case 'r': *out_keycode = KEY_R; return true;
    case 's': *out_keycode = KEY_S; return true;
    case 't': *out_keycode = KEY_T; return true;
    case 'u': *out_keycode = KEY_U; return true;
    case 'v': *out_keycode = KEY_V; return true;
    case 'w': *out_keycode = KEY_W; return true;
    case 'x': *out_keycode = KEY_X; return true;
    case 'y': *out_keycode = KEY_Y; return true;
    case 'z': *out_keycode = KEY_Z; return true;
    default: return false;
    }
}

static bool ascii_key_from_codepoint(uint32_t codepoint, Linux_Ascii_Key *out_key)
{
    if (out_key == NULL || codepoint > 0x7F) {
        return false;
    }

    memset(out_key, 0, sizeof(*out_key));

    if (codepoint >= 'a' && codepoint <= 'z') {
        return letter_keycode(codepoint, &out_key->evdev_keycode);
    }
    if (codepoint >= 'A' && codepoint <= 'Z') {
        if (!letter_keycode((uint32_t)tolower((int)codepoint), &out_key->evdev_keycode)) {
            return false;
        }
        out_key->shift = true;
        return true;
    }

    switch (codepoint) {
    case '1': out_key->evdev_keycode = KEY_1; return true;
    case '2': out_key->evdev_keycode = KEY_2; return true;
    case '3': out_key->evdev_keycode = KEY_3; return true;
    case '4': out_key->evdev_keycode = KEY_4; return true;
    case '5': out_key->evdev_keycode = KEY_5; return true;
    case '6': out_key->evdev_keycode = KEY_6; return true;
    case '7': out_key->evdev_keycode = KEY_7; return true;
    case '8': out_key->evdev_keycode = KEY_8; return true;
    case '9': out_key->evdev_keycode = KEY_9; return true;
    case '0': out_key->evdev_keycode = KEY_0; return true;
    case '!': out_key->evdev_keycode = KEY_1; out_key->shift = true; return true;
    case '@': out_key->evdev_keycode = KEY_2; out_key->shift = true; return true;
    case '#': out_key->evdev_keycode = KEY_3; out_key->shift = true; return true;
    case '$': out_key->evdev_keycode = KEY_4; out_key->shift = true; return true;
    case '%': out_key->evdev_keycode = KEY_5; out_key->shift = true; return true;
    case '^': out_key->evdev_keycode = KEY_6; out_key->shift = true; return true;
    case '&': out_key->evdev_keycode = KEY_7; out_key->shift = true; return true;
    case '*': out_key->evdev_keycode = KEY_8; out_key->shift = true; return true;
    case '(': out_key->evdev_keycode = KEY_9; out_key->shift = true; return true;
    case ')': out_key->evdev_keycode = KEY_0; out_key->shift = true; return true;
    case ' ': out_key->evdev_keycode = KEY_SPACE; return true;
    case '\n': out_key->evdev_keycode = KEY_ENTER; return true;
    case '\t': out_key->evdev_keycode = KEY_TAB; return true;
    case '-': out_key->evdev_keycode = KEY_MINUS; return true;
    case '_': out_key->evdev_keycode = KEY_MINUS; out_key->shift = true; return true;
    case '=': out_key->evdev_keycode = KEY_EQUAL; return true;
    case '+': out_key->evdev_keycode = KEY_EQUAL; out_key->shift = true; return true;
    case '[': out_key->evdev_keycode = KEY_LEFTBRACE; return true;
    case '{': out_key->evdev_keycode = KEY_LEFTBRACE; out_key->shift = true; return true;
    case ']': out_key->evdev_keycode = KEY_RIGHTBRACE; return true;
    case '}': out_key->evdev_keycode = KEY_RIGHTBRACE; out_key->shift = true; return true;
    case '\\': out_key->evdev_keycode = KEY_BACKSLASH; return true;
    case '|': out_key->evdev_keycode = KEY_BACKSLASH; out_key->shift = true; return true;
    case ';': out_key->evdev_keycode = KEY_SEMICOLON; return true;
    case ':': out_key->evdev_keycode = KEY_SEMICOLON; out_key->shift = true; return true;
    case '\'': out_key->evdev_keycode = KEY_APOSTROPHE; return true;
    case '"': out_key->evdev_keycode = KEY_APOSTROPHE; out_key->shift = true; return true;
    case ',': out_key->evdev_keycode = KEY_COMMA; return true;
    case '<': out_key->evdev_keycode = KEY_COMMA; out_key->shift = true; return true;
    case '.': out_key->evdev_keycode = KEY_DOT; return true;
    case '>': out_key->evdev_keycode = KEY_DOT; out_key->shift = true; return true;
    case '/': out_key->evdev_keycode = KEY_SLASH; return true;
    case '?': out_key->evdev_keycode = KEY_SLASH; out_key->shift = true; return true;
    case '`': out_key->evdev_keycode = KEY_GRAVE; return true;
    case '~': out_key->evdev_keycode = KEY_GRAVE; out_key->shift = true; return true;
    default: return false;
    }
}

static bool send_ascii_key(const Linux_Ascii_Key *key)
{
    if (key == NULL) {
        return false;
    }

    bool ok = true;
    if (key->shift) {
        ok = linux_uinput_emit_key_event(KEY_LEFTSHIFT, 1) && ok;
    }
    ok = linux_uinput_tap_key(key->evdev_keycode) && ok;
    if (key->shift) {
        ok = linux_uinput_emit_key_event(KEY_LEFTSHIFT, 0) && ok;
    }
    return ok;
}

bool platform_output_init(void)
{
    if (g_linux.uinput_fd >= 0) {
        return true;
    }

    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fputs("stoin: failed to open /dev/uinput for Linux keyboard output\n", stderr);
        fputs("stoin: check uinput permissions or run with access to /dev/uinput\n", stderr);
        return false;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0 || ioctl(fd, UI_SET_EVBIT, EV_SYN) != 0) {
        close(fd);
        return false;
    }

    for (unsigned int key = 1; key < KEY_MAX; ++key) {
        (void)ioctl(fd, UI_SET_KEYBIT, key);
    }

    struct uinput_setup setup = {0};
    snprintf(setup.name, sizeof(setup.name), "%s", STOIN_LINUX_UINPUT_NAME);
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x7374;
    setup.id.product = 0x6f69;
    setup.id.version = 1;

    if (ioctl(fd, UI_DEV_SETUP, &setup) != 0 || ioctl(fd, UI_DEV_CREATE) != 0) {
        close(fd);
        return false;
    }

    g_linux.uinput_fd = fd;
    platform_sleep_ms(100);
    return true;
}

void linux_output_shutdown(void)
{
    if (g_linux.uinput_fd < 0) {
        return;
    }

    (void)ioctl(g_linux.uinput_fd, UI_DEV_DESTROY);
    close(g_linux.uinput_fd);
    g_linux.uinput_fd = -1;
}

static bool combo_token_equals(const char *start, size_t length, const char *expected)
{
    if (strlen(expected) != length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)expected[i])) {
            return false;
        }
    }
    return true;
}

static const char *skip_combo_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static bool combo_modifier_flag(const char *start, size_t length, unsigned int *out_flag)
{
    if (combo_token_equals(start, length, "shift")
        || combo_token_equals(start, length, "shift_l")
        || combo_token_equals(start, length, "shift_r")) {
        *out_flag = LINUX_COMBO_SHIFT;
        return true;
    }
    if (combo_token_equals(start, length, "control")
        || combo_token_equals(start, length, "control_l")
        || combo_token_equals(start, length, "control_r")
        || combo_token_equals(start, length, "ctrl")) {
        *out_flag = LINUX_COMBO_CONTROL;
        return true;
    }
    if (combo_token_equals(start, length, "alt")
        || combo_token_equals(start, length, "alt_l")
        || combo_token_equals(start, length, "alt_r")
        || combo_token_equals(start, length, "option")
        || combo_token_equals(start, length, "option_l")
        || combo_token_equals(start, length, "option_r")) {
        *out_flag = LINUX_COMBO_ALT;
        return true;
    }
    if (combo_token_equals(start, length, "command")
        || combo_token_equals(start, length, "super")
        || combo_token_equals(start, length, "super_l")
        || combo_token_equals(start, length, "super_r")
        || combo_token_equals(start, length, "windows")) {
        *out_flag = LINUX_COMBO_SUPER;
        return true;
    }
    return false;
}

static bool evdev_keycode_for_function_key(int function_key, unsigned int *out_keycode)
{
    if (out_keycode == NULL) {
        return false;
    }

    switch (function_key) {
    case 1: *out_keycode = KEY_F1; return true;
    case 2: *out_keycode = KEY_F2; return true;
    case 3: *out_keycode = KEY_F3; return true;
    case 4: *out_keycode = KEY_F4; return true;
    case 5: *out_keycode = KEY_F5; return true;
    case 6: *out_keycode = KEY_F6; return true;
    case 7: *out_keycode = KEY_F7; return true;
    case 8: *out_keycode = KEY_F8; return true;
    case 9: *out_keycode = KEY_F9; return true;
    case 10: *out_keycode = KEY_F10; return true;
    case 11: *out_keycode = KEY_F11; return true;
    case 12: *out_keycode = KEY_F12; return true;
    default: return false;
    }
}

static bool combo_keycode_from_token(const char *start, size_t length, unsigned int *out_keycode)
{
    if (out_keycode == NULL) {
        return false;
    }

    if (length == 1) {
        char key_name[2] = { (char)tolower((unsigned char)start[0]), '\0' };
        uint16_t logical_keycode = 0;
        if (platform_keycode_from_name(key_name, &logical_keycode)
            && linux_evdev_keycode_from_logical(logical_keycode, out_keycode)) {
            return true;
        }
    }

    if (combo_token_equals(start, length, "home")) {
        *out_keycode = KEY_HOME;
    } else if (combo_token_equals(start, length, "end")) {
        *out_keycode = KEY_END;
    } else if (combo_token_equals(start, length, "page_up") || combo_token_equals(start, length, "pageup")) {
        *out_keycode = KEY_PAGEUP;
    } else if (combo_token_equals(start, length, "page_down") || combo_token_equals(start, length, "pagedown")) {
        *out_keycode = KEY_PAGEDOWN;
    } else if (combo_token_equals(start, length, "left")) {
        *out_keycode = KEY_LEFT;
    } else if (combo_token_equals(start, length, "right")) {
        *out_keycode = KEY_RIGHT;
    } else if (combo_token_equals(start, length, "down")) {
        *out_keycode = KEY_DOWN;
    } else if (combo_token_equals(start, length, "up")) {
        *out_keycode = KEY_UP;
    } else if (combo_token_equals(start, length, "tab")) {
        *out_keycode = KEY_TAB;
    } else if (combo_token_equals(start, length, "return") || combo_token_equals(start, length, "enter")) {
        *out_keycode = KEY_ENTER;
    } else if (combo_token_equals(start, length, "backspace")) {
        *out_keycode = KEY_BACKSPACE;
    } else if (combo_token_equals(start, length, "delete")) {
        *out_keycode = KEY_DELETE;
    } else if (combo_token_equals(start, length, "escape") || combo_token_equals(start, length, "esc")) {
        *out_keycode = KEY_ESC;
    } else if (length >= 2 && (start[0] == 'F' || start[0] == 'f') && isdigit((unsigned char)start[1])) {
        char buffer[4] = {0};
        if (length >= sizeof(buffer)) {
            return false;
        }
        memcpy(buffer, start + 1, length - 1);
        return evdev_keycode_for_function_key(atoi(buffer), out_keycode);
    } else {
        return false;
    }

    return true;
}

static bool parse_combo_expression(const char **cursor, unsigned int *flags, unsigned int *out_keycode)
{
    const char *p = skip_combo_ws(*cursor);
    const char *token = p;
    while (isalnum((unsigned char)*p) || *p == '_') {
        ++p;
    }
    const size_t token_length = (size_t)(p - token);
    if (token_length == 0) {
        return false;
    }

    p = skip_combo_ws(p);
    if (*p == '(') {
        unsigned int flag = 0;
        if (!combo_modifier_flag(token, token_length, &flag)) {
            return false;
        }
        *flags |= flag;
        ++p;
        if (!parse_combo_expression(&p, flags, out_keycode)) {
            return false;
        }
        p = skip_combo_ws(p);
        if (*p != ')') {
            return false;
        }
        *cursor = p + 1;
        return true;
    }

    if (!combo_keycode_from_token(token, token_length, out_keycode)) {
        return false;
    }
    *cursor = p;
    return true;
}

static bool emit_modifier(unsigned int modifier, bool is_down)
{
    switch (modifier) {
    case LINUX_COMBO_SHIFT:
        return linux_uinput_emit_key_event(KEY_LEFTSHIFT, is_down ? 1 : 0);
    case LINUX_COMBO_CONTROL:
        return linux_uinput_emit_key_event(KEY_LEFTCTRL, is_down ? 1 : 0);
    case LINUX_COMBO_ALT:
        return linux_uinput_emit_key_event(KEY_LEFTALT, is_down ? 1 : 0);
    case LINUX_COMBO_SUPER:
        return linux_uinput_emit_key_event(KEY_LEFTMETA, is_down ? 1 : 0);
    default:
        return true;
    }
}

static bool emit_key_combination(unsigned int modifiers, unsigned int keycode)
{
    bool ok = true;
    if ((modifiers & LINUX_COMBO_SHIFT) != 0) {
        ok = emit_modifier(LINUX_COMBO_SHIFT, true) && ok;
    }
    if ((modifiers & LINUX_COMBO_CONTROL) != 0) {
        ok = emit_modifier(LINUX_COMBO_CONTROL, true) && ok;
    }
    if ((modifiers & LINUX_COMBO_ALT) != 0) {
        ok = emit_modifier(LINUX_COMBO_ALT, true) && ok;
    }
    if ((modifiers & LINUX_COMBO_SUPER) != 0) {
        ok = emit_modifier(LINUX_COMBO_SUPER, true) && ok;
    }

    ok = linux_uinput_tap_key(keycode) && ok;

    if ((modifiers & LINUX_COMBO_SUPER) != 0) {
        ok = emit_modifier(LINUX_COMBO_SUPER, false) && ok;
    }
    if ((modifiers & LINUX_COMBO_ALT) != 0) {
        ok = emit_modifier(LINUX_COMBO_ALT, false) && ok;
    }
    if ((modifiers & LINUX_COMBO_CONTROL) != 0) {
        ok = emit_modifier(LINUX_COMBO_CONTROL, false) && ok;
    }
    if ((modifiers & LINUX_COMBO_SHIFT) != 0) {
        ok = emit_modifier(LINUX_COMBO_SHIFT, false) && ok;
    }

    return ok;
}

bool platform_send_key_combination(const char *combo)
{
    if (combo == NULL) {
        return false;
    }
    if (!platform_user_session_is_active()) {
        return false;
    }
    if (!platform_output_init()) {
        return false;
    }

    const char *p = combo;
    unsigned int flags = 0;
    unsigned int keycode = 0;
    if (!parse_combo_expression(&p, &flags, &keycode)) {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }
    p = skip_combo_ws(p);
    if (*p != '\0') {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }

    linux_report_translation_timing_before_output("key-combo");
    return emit_key_combination(flags, keycode);
}

static bool utf8_next_codepoint(const char **cursor, uint32_t *out_codepoint)
{
    if (cursor == NULL || *cursor == NULL || out_codepoint == NULL) {
        return false;
    }

    const unsigned char *p = (const unsigned char *)*cursor;
    if (*p == '\0') {
        return false;
    }

    uint32_t codepoint = 0;
    size_t length = 0;
    if (p[0] < 0x80) {
        codepoint = p[0];
        length = 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        codepoint = p[0] & 0x1F;
        length = 2;
        if (codepoint == 0) {
            return false;
        }
    } else if ((p[0] & 0xF0) == 0xE0) {
        codepoint = p[0] & 0x0F;
        length = 3;
    } else if ((p[0] & 0xF8) == 0xF0) {
        codepoint = p[0] & 0x07;
        length = 4;
    } else {
        return false;
    }

    for (size_t i = 1; i < length; ++i) {
        if ((p[i] & 0xC0) != 0x80) {
            return false;
        }
        codepoint = (codepoint << 6) | (uint32_t)(p[i] & 0x3F);
    }

    if ((length == 2 && codepoint < 0x80)
        || (length == 3 && codepoint < 0x800)
        || (length == 4 && codepoint < 0x10000)
        || (codepoint >= 0xD800 && codepoint <= 0xDFFF)
        || codepoint > 0x10FFFF) {
        return false;
    }

    *cursor += length;
    *out_codepoint = codepoint;
    return true;
}

static bool hex_digit_keycode(char digit, unsigned int *out_keycode)
{
    if (out_keycode == NULL) {
        return false;
    }

    switch (digit) {
    case '0': *out_keycode = KEY_0; return true;
    case '1': *out_keycode = KEY_1; return true;
    case '2': *out_keycode = KEY_2; return true;
    case '3': *out_keycode = KEY_3; return true;
    case '4': *out_keycode = KEY_4; return true;
    case '5': *out_keycode = KEY_5; return true;
    case '6': *out_keycode = KEY_6; return true;
    case '7': *out_keycode = KEY_7; return true;
    case '8': *out_keycode = KEY_8; return true;
    case '9': *out_keycode = KEY_9; return true;
    case 'a': *out_keycode = KEY_A; return true;
    case 'b': *out_keycode = KEY_B; return true;
    case 'c': *out_keycode = KEY_C; return true;
    case 'd': *out_keycode = KEY_D; return true;
    case 'e': *out_keycode = KEY_E; return true;
    case 'f': *out_keycode = KEY_F; return true;
    default: return false;
    }
}

static bool send_unicode_codepoint(uint32_t codepoint)
{
    char hex[9] = {0};
    snprintf(hex, sizeof(hex), "%x", codepoint);

    bool ok = true;
    ok = linux_uinput_emit_key_event(KEY_LEFTCTRL, 1) && ok;
    ok = linux_uinput_emit_key_event(KEY_LEFTSHIFT, 1) && ok;
    ok = linux_uinput_tap_key(KEY_U) && ok;
    ok = linux_uinput_emit_key_event(KEY_LEFTSHIFT, 0) && ok;
    ok = linux_uinput_emit_key_event(KEY_LEFTCTRL, 0) && ok;
    if (!ok) {
        return false;
    }

    for (const char *p = hex; ok && *p != '\0'; ++p) {
        unsigned int keycode = 0;
        ok = hex_digit_keycode(*p, &keycode) && linux_uinput_tap_key(keycode);
    }

    return ok && linux_uinput_tap_key(KEY_SPACE);
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

    const char *cursor = utf8;
    bool reported_timing = false;
    while (*cursor != '\0') {
        uint32_t codepoint = 0;
        if (!utf8_next_codepoint(&cursor, &codepoint)) {
            return false;
        }
        if (!reported_timing) {
            linux_report_translation_timing_before_output("text");
            reported_timing = true;
        }

        Linux_Ascii_Key ascii_key = {0};
        if (ascii_key_from_codepoint(codepoint, &ascii_key)) {
            if (!send_ascii_key(&ascii_key)) {
                return false;
            }
        } else if (!send_unicode_codepoint(codepoint)) {
            return false;
        }
    }

    return true;
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

    size_t character_count = 0;
    const char *cursor = utf8;
    while (*cursor != '\0') {
        uint32_t codepoint = 0;
        if (!utf8_next_codepoint(&cursor, &codepoint)) {
            return false;
        }
        ++character_count;
    }

    if (character_count > 0) {
        linux_report_translation_timing_before_output("delete");
    }

    for (size_t i = 0; i < character_count; ++i) {
        if (!linux_uinput_tap_key(KEY_BACKSPACE)) {
            return false;
        }
    }

    return true;
}
