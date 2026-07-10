#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

#define STOIN_WINDOWS_EXTRA_INFO ((ULONG_PTR)0x73746f696eULL)
#define WINDOWS_FILE_WATCH_POLL_MS 250
#define WINDOWS_SERIAL_MAX_CANDIDATES 256

typedef enum Windows_Combo_Modifier {
    WINDOWS_COMBO_SHIFT = 1 << 0,
    WINDOWS_COMBO_CONTROL = 1 << 1,
    WINDOWS_COMBO_ALT = 1 << 2,
    WINDOWS_COMBO_SUPER = 1 << 3,
} Windows_Combo_Modifier;

typedef struct Windows_Key_Map {
    const char *name;
    uint16_t logical_keycode;
    UINT vk;
    char printable;
} Windows_Key_Map;

typedef struct Windows_File_Watch_Target {
    char *path;
    Platform_File_Stamp stamp;
} Windows_File_Watch_Target;

typedef struct Windows_Serial_Candidate {
    char name[16];
    char path[PLATFORM_SERIAL_PATH_MAX];
    char friendly_name[256];
    char description[256];
    char manufacturer[256];
    int score;
    unsigned int number;
    bool exists;
} Windows_Serial_Candidate;

typedef struct Windows_State {
    HHOOK keyboard_hook;
    UINT_PTR watcher_timer_id;
    Handle_Input_Fn handler;
    void *userdata;
    Platform_File_Watch_Fn file_watcher_callback;
    void *file_watcher_userdata;
    Windows_File_Watch_Target *file_watcher_targets;
    uint64_t performance_frequency;
    uint64_t translation_timing_start_ns;
    uint64_t translation_timing_sequence;
    bool down_vks[256];
    bool running;
    bool shift_down;
    bool control_down;
    bool option_down;
    bool command_down;
    bool file_watcher_active;
    bool translation_timing_enabled;
    bool translation_timing_active;
} Windows_State;

struct Platform_Serial_Port {
    HANDLE handle;
    char port_path[PLATFORM_SERIAL_PATH_MAX];
    bool had_error;
};

static Windows_State g_windows;

static const Windows_Key_Map WINDOWS_KEY_MAP[] = {
    {"a", 0, 'A', 'a'},
    {"s", 1, 'S', 's'},
    {"d", 2, 'D', 'd'},
    {"f", 3, 'F', 'f'},
    {"h", 4, 'H', 'h'},
    {"g", 5, 'G', 'g'},
    {"z", 6, 'Z', 'z'},
    {"x", 7, 'X', 'x'},
    {"c", 8, 'C', 'c'},
    {"v", 9, 'V', 'v'},
    {"b", 11, 'B', 'b'},
    {"q", 12, 'Q', 'q'},
    {"w", 13, 'W', 'w'},
    {"e", 14, 'E', 'e'},
    {"r", 15, 'R', 'r'},
    {"y", 16, 'Y', 'y'},
    {"t", 17, 'T', 't'},
    {"1", 18, '1', '1'},
    {"2", 19, '2', '2'},
    {"3", 20, '3', '3'},
    {"4", 21, '4', '4'},
    {"6", 22, '6', '6'},
    {"5", 23, '5', '5'},
    {"9", 25, '9', '9'},
    {"7", 26, '7', '7'},
    {"8", 28, '8', '8'},
    {"0", 29, '0', '0'},
    {"]", 30, VK_OEM_6, ']'},
    {"o", 31, 'O', 'o'},
    {"u", 32, 'U', 'u'},
    {"[", 33, VK_OEM_4, '['},
    {"i", 34, 'I', 'i'},
    {"p", 35, 'P', 'p'},
    {"l", 37, 'L', 'l'},
    {"j", 38, 'J', 'j'},
    {"'", 39, VK_OEM_7, '\''},
    {"apostrophe", 39, VK_OEM_7, '\''},
    {"quote", 39, VK_OEM_7, '\''},
    {"k", 40, 'K', 'k'},
    {";", 41, VK_OEM_1, ';'},
    {"semicolon", 41, VK_OEM_1, ';'},
    {"\\", 42, VK_OEM_5, '\\'},
    {"backslash", 42, VK_OEM_5, '\\'},
    {",", 43, VK_OEM_COMMA, ','},
    {"comma", 43, VK_OEM_COMMA, ','},
    {"/", 44, VK_OEM_2, '/'},
    {"slash", 44, VK_OEM_2, '/'},
    {"n", 45, 'N', 'n'},
    {"m", 46, 'M', 'm'},
    {".", 47, VK_OEM_PERIOD, '.'},
    {"period", 47, VK_OEM_PERIOD, '.'},
    {"dot", 47, VK_OEM_PERIOD, '.'},
    {"tab", 48, VK_TAB, '\0'},
    {"space", 49, VK_SPACE, ' '},
    {"`", 50, VK_OEM_3, '`'},
    {"backspace", 51, VK_BACK, '\0'},
    {"enter", 36, VK_RETURN, '\0'},
    {"return", 36, VK_RETURN, '\0'},
    {"escape", 53, VK_ESCAPE, '\0'},
    {"esc", 53, VK_ESCAPE, '\0'},
    {"right_command", 54, VK_RWIN, '\0'},
    {"right_super", 54, VK_RWIN, '\0'},
    {"right_windows", 54, VK_RWIN, '\0'},
    {"left_command", 55, VK_LWIN, '\0'},
    {"left_super", 55, VK_LWIN, '\0'},
    {"left_windows", 55, VK_LWIN, '\0'},
    {"left_shift", 56, VK_LSHIFT, '\0'},
    {"left_option", 58, VK_LMENU, '\0'},
    {"left_alt", 58, VK_LMENU, '\0'},
    {"left_control", 59, VK_LCONTROL, '\0'},
    {"left_ctrl", 59, VK_LCONTROL, '\0'},
    {"right_shift", 60, VK_RSHIFT, '\0'},
    {"right_option", 61, VK_RMENU, '\0'},
    {"right_alt", 61, VK_RMENU, '\0'},
    {"right_control", 62, VK_RCONTROL, '\0'},
    {"right_ctrl", 62, VK_RCONTROL, '\0'},
    {"f13", 105, VK_F13, '\0'},
    {"f14", 107, VK_F14, '\0'},
    {"f15", 113, VK_F15, '\0'},
    {"f16", 106, VK_F16, '\0'},
    {"f17", 64, VK_F17, '\0'},
    {"f18", 79, VK_F18, '\0'},
    {"f19", 80, VK_F19, '\0'},
    {"f20", 90, VK_F20, '\0'},
};

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

static char *windows_copy_range_cstring(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *windows_copy_cstring(const char *s)
{
    return s == NULL ? NULL : windows_copy_range_cstring(s, strlen(s));
}

static bool windows_vk_from_logical(uint16_t logical_keycode, UINT *out_vk)
{
    if (out_vk == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(WINDOWS_KEY_MAP) / sizeof(WINDOWS_KEY_MAP[0]); ++i) {
        if (WINDOWS_KEY_MAP[i].logical_keycode == logical_keycode) {
            *out_vk = WINDOWS_KEY_MAP[i].vk;
            return true;
        }
    }
    return false;
}

static bool windows_logical_from_vk(UINT vk, uint16_t *out_logical_keycode)
{
    if (out_logical_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(WINDOWS_KEY_MAP) / sizeof(WINDOWS_KEY_MAP[0]); ++i) {
        if (WINDOWS_KEY_MAP[i].vk == vk) {
            *out_logical_keycode = WINDOWS_KEY_MAP[i].logical_keycode;
            return true;
        }
    }
    return false;
}

static char windows_printable_from_logical(uint16_t logical_keycode)
{
    for (size_t i = 0; i < sizeof(WINDOWS_KEY_MAP) / sizeof(WINDOWS_KEY_MAP[0]); ++i) {
        if (WINDOWS_KEY_MAP[i].logical_keycode == logical_keycode && WINDOWS_KEY_MAP[i].printable != '\0') {
            return WINDOWS_KEY_MAP[i].printable;
        }
    }
    return '\0';
}

bool platform_keycode_from_name(const char *name, uint16_t *out_keycode)
{
    if (name == NULL || out_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(WINDOWS_KEY_MAP) / sizeof(WINDOWS_KEY_MAP[0]); ++i) {
        if (key_name_equals(name, WINDOWS_KEY_MAP[i].name)) {
            *out_keycode = WINDOWS_KEY_MAP[i].logical_keycode;
            return true;
        }
    }
    return false;
}

static UINT normalize_hook_vk(const KBDLLHOOKSTRUCT *event)
{
    if (event == NULL) {
        return 0;
    }

    switch (event->vkCode) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    {
        UINT scan_code = event->scanCode;
        if ((event->flags & LLKHF_EXTENDED) != 0) {
            scan_code |= 0xE000;
        }
        return MapVirtualKeyA(scan_code, MAPVK_VSC_TO_VK_EX);
    }
    default:
        return event->vkCode;
    }
}

static void update_modifier_state(uint16_t logical_keycode, bool is_down)
{
    switch (logical_keycode) {
    case 54:
    case 55:
        g_windows.command_down = is_down;
        break;
    case 56:
    case 60:
        g_windows.shift_down = is_down;
        break;
    case 58:
    case 61:
        g_windows.option_down = is_down;
        break;
    case 59:
    case 62:
        g_windows.control_down = is_down;
        break;
    default:
        break;
    }
}

static LRESULT CALLBACK keyboard_hook_callback(int code, WPARAM wparam, LPARAM lparam)
{
    if (code < 0 || lparam == 0) {
        return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
    }

    const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)lparam;
    if (event->dwExtraInfo == STOIN_WINDOWS_EXTRA_INFO) {
        return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
    }

    const bool is_down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
    const bool is_up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
    if (!is_down && !is_up) {
        return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
    }

    const UINT vk = normalize_hook_vk(event);
    if (vk >= sizeof(g_windows.down_vks) / sizeof(g_windows.down_vks[0])) {
        return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
    }

    uint16_t logical_keycode = 0;
    if (!windows_logical_from_vk(vk, &logical_keycode)) {
        return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
    }

    const bool repeat = is_down && g_windows.down_vks[vk];
    g_windows.down_vks[vk] = is_down;
    if (!repeat) {
        update_modifier_state(logical_keycode, is_down);
    }

    bool consumed = false;
    if (g_windows.handler != NULL) {
        const Input_Event input = {
            .keycode = logical_keycode,
            .is_down = is_down,
            .is_repeat = repeat,
            .shift = g_windows.shift_down,
            .control = g_windows.control_down,
            .option = g_windows.option_down,
            .command = g_windows.command_down,
            .printable = windows_printable_from_logical(logical_keycode),
        };
        consumed = g_windows.handler(&input, g_windows.userdata);
    }

    if (consumed) {
        return 1;
    }

    return CallNextHookEx(g_windows.keyboard_hook, code, wparam, lparam);
}

bool platform_user_session_is_active(void)
{
    return true;
}

void platform_translation_timing_set_enabled(bool enabled)
{
    g_windows.translation_timing_enabled = enabled;
    if (!enabled) {
        g_windows.translation_timing_active = false;
        g_windows.translation_timing_start_ns = 0;
    }
}

void platform_translation_timing_begin(uint64_t start_ns)
{
    if (!g_windows.translation_timing_enabled || start_ns == 0) {
        return;
    }

    g_windows.translation_timing_start_ns = start_ns;
    g_windows.translation_timing_active = true;
}

void platform_translation_timing_cancel(void)
{
    g_windows.translation_timing_active = false;
    g_windows.translation_timing_start_ns = 0;
}

static void windows_report_translation_timing_before_output(const char *operation)
{
    if (!g_windows.translation_timing_enabled || !g_windows.translation_timing_active) {
        return;
    }

    const uint64_t now_ns = platform_monotonic_ns();
    const uint64_t start_ns = g_windows.translation_timing_start_ns;
    const uint64_t elapsed_ns = now_ns >= start_ns ? now_ns - start_ns : 0;
    g_windows.translation_timing_active = false;
    g_windows.translation_timing_start_ns = 0;
    ++g_windows.translation_timing_sequence;

    fprintf(stderr,
        "stoin: translation latency #%" PRIu64 " before %s SendInput: %.3f ms (%.1f us)\n",
        g_windows.translation_timing_sequence,
        operation != NULL ? operation : "first",
        (double)elapsed_ns / 1000000.0,
        (double)elapsed_ns / 1000.0);
}

uint64_t platform_monotonic_ns(void)
{
    LARGE_INTEGER now;
    if (g_windows.performance_frequency == 0) {
        LARGE_INTEGER frequency;
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            return GetTickCount64() * UINT64_C(1000000);
        }
        g_windows.performance_frequency = (uint64_t)frequency.QuadPart;
    }

    if (!QueryPerformanceCounter(&now)) {
        return GetTickCount64() * UINT64_C(1000000);
    }

    return ((uint64_t)now.QuadPart * UINT64_C(1000000000)) / g_windows.performance_frequency;
}

uint64_t platform_monotonic_ms(void)
{
    return platform_monotonic_ns() / UINT64_C(1000000);
}

void platform_sleep_ms(unsigned int milliseconds)
{
    Sleep(milliseconds);
}

bool platform_output_init(void)
{
    return true;
}

static bool send_inputs(INPUT *inputs, UINT input_count)
{
    if (inputs == NULL || input_count == 0) {
        return false;
    }
    return SendInput(input_count, inputs, sizeof(INPUT)) == input_count;
}

static INPUT keyboard_input_vk(UINT vk, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = (WORD)vk;
    input.ki.dwFlags = flags;
    input.ki.dwExtraInfo = STOIN_WINDOWS_EXTRA_INFO;
    return input;
}

static INPUT keyboard_input_unicode(WCHAR code_unit, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = code_unit;
    input.ki.dwFlags = KEYEVENTF_UNICODE | flags;
    input.ki.dwExtraInfo = STOIN_WINDOWS_EXTRA_INFO;
    return input;
}

static bool tap_vk(UINT vk)
{
    INPUT inputs[2] = {
        keyboard_input_vk(vk, 0),
        keyboard_input_vk(vk, KEYEVENTF_KEYUP),
    };
    return send_inputs(inputs, 2);
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
        *out_flag = WINDOWS_COMBO_SHIFT;
        return true;
    }
    if (combo_token_equals(start, length, "control")
        || combo_token_equals(start, length, "control_l")
        || combo_token_equals(start, length, "control_r")
        || combo_token_equals(start, length, "ctrl")) {
        *out_flag = WINDOWS_COMBO_CONTROL;
        return true;
    }
    if (combo_token_equals(start, length, "alt")
        || combo_token_equals(start, length, "alt_l")
        || combo_token_equals(start, length, "alt_r")
        || combo_token_equals(start, length, "option")
        || combo_token_equals(start, length, "option_l")
        || combo_token_equals(start, length, "option_r")) {
        *out_flag = WINDOWS_COMBO_ALT;
        return true;
    }
    if (combo_token_equals(start, length, "command")
        || combo_token_equals(start, length, "super")
        || combo_token_equals(start, length, "super_l")
        || combo_token_equals(start, length, "super_r")
        || combo_token_equals(start, length, "windows")) {
        *out_flag = WINDOWS_COMBO_SUPER;
        return true;
    }
    return false;
}

static bool vk_for_function_key(int function_key, UINT *out_vk)
{
    if (out_vk == NULL || function_key < 1 || function_key > 24) {
        return false;
    }
    *out_vk = (UINT)(VK_F1 + function_key - 1);
    return true;
}

static bool combo_keycode_from_token(const char *start, size_t length, UINT *out_vk)
{
    if (out_vk == NULL) {
        return false;
    }

    if (length == 1) {
        char key_name[2] = { (char)tolower((unsigned char)start[0]), '\0' };
        uint16_t logical_keycode = 0;
        if (platform_keycode_from_name(key_name, &logical_keycode)
            && windows_vk_from_logical(logical_keycode, out_vk)) {
            return true;
        }
    }

    if (combo_token_equals(start, length, "home")) {
        *out_vk = VK_HOME;
    } else if (combo_token_equals(start, length, "end")) {
        *out_vk = VK_END;
    } else if (combo_token_equals(start, length, "page_up") || combo_token_equals(start, length, "pageup")) {
        *out_vk = VK_PRIOR;
    } else if (combo_token_equals(start, length, "page_down") || combo_token_equals(start, length, "pagedown")) {
        *out_vk = VK_NEXT;
    } else if (combo_token_equals(start, length, "left")) {
        *out_vk = VK_LEFT;
    } else if (combo_token_equals(start, length, "right")) {
        *out_vk = VK_RIGHT;
    } else if (combo_token_equals(start, length, "down")) {
        *out_vk = VK_DOWN;
    } else if (combo_token_equals(start, length, "up")) {
        *out_vk = VK_UP;
    } else if (combo_token_equals(start, length, "tab")) {
        *out_vk = VK_TAB;
    } else if (combo_token_equals(start, length, "return") || combo_token_equals(start, length, "enter")) {
        *out_vk = VK_RETURN;
    } else if (combo_token_equals(start, length, "backspace")) {
        *out_vk = VK_BACK;
    } else if (combo_token_equals(start, length, "delete")) {
        *out_vk = VK_DELETE;
    } else if (combo_token_equals(start, length, "escape") || combo_token_equals(start, length, "esc")) {
        *out_vk = VK_ESCAPE;
    } else if (length >= 2 && (start[0] == 'F' || start[0] == 'f') && isdigit((unsigned char)start[1])) {
        char buffer[4] = {0};
        if (length >= sizeof(buffer)) {
            return false;
        }
        memcpy(buffer, start + 1, length - 1);
        return vk_for_function_key(atoi(buffer), out_vk);
    } else {
        return false;
    }

    return true;
}

static bool parse_combo_expression(const char **cursor, unsigned int *modifiers, UINT *out_vk)
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
        *modifiers |= flag;
        ++p;
        if (!parse_combo_expression(&p, modifiers, out_vk)) {
            return false;
        }
        p = skip_combo_ws(p);
        if (*p != ')') {
            return false;
        }
        *cursor = p + 1;
        return true;
    }

    if (!combo_keycode_from_token(token, token_length, out_vk)) {
        return false;
    }
    *cursor = p;
    return true;
}

static UINT modifier_vk(unsigned int modifier)
{
    switch (modifier) {
    case WINDOWS_COMBO_SHIFT: return VK_SHIFT;
    case WINDOWS_COMBO_CONTROL: return VK_CONTROL;
    case WINDOWS_COMBO_ALT: return VK_MENU;
    case WINDOWS_COMBO_SUPER: return VK_LWIN;
    default: return 0;
    }
}

static bool emit_modifier(unsigned int modifier, bool is_down)
{
    const UINT vk = modifier_vk(modifier);
    if (vk == 0) {
        return true;
    }
    INPUT input = keyboard_input_vk(vk, is_down ? 0 : KEYEVENTF_KEYUP);
    return send_inputs(&input, 1);
}

static bool emit_key_combination(unsigned int modifiers, UINT vk)
{
    bool ok = true;
    if ((modifiers & WINDOWS_COMBO_SHIFT) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_SHIFT, true) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_CONTROL) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_CONTROL, true) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_ALT) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_ALT, true) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_SUPER) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_SUPER, true) && ok;
    }

    ok = tap_vk(vk) && ok;

    if ((modifiers & WINDOWS_COMBO_SUPER) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_SUPER, false) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_ALT) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_ALT, false) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_CONTROL) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_CONTROL, false) && ok;
    }
    if ((modifiers & WINDOWS_COMBO_SHIFT) != 0) {
        ok = emit_modifier(WINDOWS_COMBO_SHIFT, false) && ok;
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
    unsigned int modifiers = 0;
    UINT vk = 0;
    if (!parse_combo_expression(&p, &modifiers, &vk)) {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }
    p = skip_combo_ws(p);
    if (*p != '\0') {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }

    windows_report_translation_timing_before_output("key-combo");
    return emit_key_combination(modifiers, vk);
}

static WCHAR *utf8_to_utf16(const char *utf8, int *out_length)
{
    if (out_length != NULL) {
        *out_length = 0;
    }
    if (utf8 == NULL || out_length == NULL) {
        return NULL;
    }

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (required <= 0) {
        return NULL;
    }

    WCHAR *wide = calloc((size_t)required, sizeof(*wide));
    if (wide == NULL) {
        return NULL;
    }

    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, required);
    if (written != required) {
        free(wide);
        return NULL;
    }

    *out_length = required - 1;
    return wide;
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

    int length = 0;
    WCHAR *wide = utf8_to_utf16(utf8, &length);
    if (wide == NULL) {
        return false;
    }

    bool ok = true;
    bool reported_timing = false;
    for (int i = 0; i < length; ++i) {
        INPUT inputs[2] = {
            keyboard_input_unicode(wide[i], 0),
            keyboard_input_unicode(wide[i], KEYEVENTF_KEYUP),
        };
        if (!reported_timing) {
            windows_report_translation_timing_before_output("text");
            reported_timing = true;
        }
        ok = send_inputs(inputs, 2) && ok;
    }

    free(wide);
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

    int length = 0;
    WCHAR *wide = utf8_to_utf16(utf8, &length);
    if (wide == NULL) {
        return false;
    }

    size_t character_count = 0;
    for (int i = 0; i < length; ++i) {
        if (wide[i] >= 0xDC00 && wide[i] <= 0xDFFF) {
            continue;
        }
        ++character_count;
    }
    free(wide);

    if (character_count > 0) {
        windows_report_translation_timing_before_output("delete");
    }
    for (size_t i = 0; i < character_count; ++i) {
        if (!tap_vk(VK_BACK)) {
            return false;
        }
    }

    return true;
}

bool platform_file_stamp(const char *path, Platform_File_Stamp *out_stamp)
{
    if (path == NULL || out_stamp == NULL) {
        return false;
    }

    memset(out_stamp, 0, sizeof(*out_stamp));

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        return false;
    }

    ULARGE_INTEGER modified;
    modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
    modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
    ULARGE_INTEGER size;
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;

    out_stamp->exists = true;
    out_stamp->size = size.QuadPart;
    out_stamp->modified_time_ns = modified.QuadPart * UINT64_C(100);
    return true;
}

static bool file_stamps_equal(Platform_File_Stamp a, Platform_File_Stamp b)
{
    return a.exists == b.exists
        && a.size == b.size
        && a.modified_time_ns == b.modified_time_ns;
}

static void clear_file_watcher_targets(void)
{
    for (size_t i = 0; i < arrlenu(g_windows.file_watcher_targets); ++i) {
        free(g_windows.file_watcher_targets[i].path);
    }
    arrfree(g_windows.file_watcher_targets);
    g_windows.file_watcher_targets = NULL;
}

void platform_file_watcher_poll(void)
{
    if (!g_windows.file_watcher_active) {
        return;
    }

    bool changed = false;
    for (size_t i = 0; i < arrlenu(g_windows.file_watcher_targets); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(g_windows.file_watcher_targets[i].path, &stamp)) {
            continue;
        }
        if (!file_stamps_equal(stamp, g_windows.file_watcher_targets[i].stamp)) {
            g_windows.file_watcher_targets[i].stamp = stamp;
            changed = true;
        }
    }

    if (changed && g_windows.file_watcher_callback != NULL) {
        g_windows.file_watcher_callback(g_windows.file_watcher_userdata);
    }
}

bool platform_file_watcher_start(
    const char *const *paths,
    size_t path_count,
    Platform_File_Watch_Fn callback,
    void *userdata
)
{
    if (paths == NULL || path_count == 0 || callback == NULL) {
        return false;
    }

    platform_file_watcher_stop();

    for (size_t i = 0; i < path_count; ++i) {
        char *copy = windows_copy_cstring(paths[i]);
        if (copy == NULL) {
            platform_file_watcher_stop();
            return false;
        }

        Platform_File_Stamp stamp = {0};
        (void)platform_file_stamp(copy, &stamp);
        Windows_File_Watch_Target target = {
            .path = copy,
            .stamp = stamp,
        };
        arrput(g_windows.file_watcher_targets, target);
    }

    g_windows.file_watcher_active = true;
    g_windows.file_watcher_callback = callback;
    g_windows.file_watcher_userdata = userdata;
    return true;
}

void platform_file_watcher_stop(void)
{
    clear_file_watcher_targets();
    g_windows.file_watcher_active = false;
    g_windows.file_watcher_callback = NULL;
    g_windows.file_watcher_userdata = NULL;
}

bool platform_init(Handle_Input_Fn handler, void *userdata)
{
    g_windows.handler = handler;
    g_windows.userdata = userdata;

    if (!platform_output_init()) {
        return false;
    }

    g_windows.keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL, keyboard_hook_callback, GetModuleHandleA(NULL), 0);
    if (g_windows.keyboard_hook == NULL) {
        fputs("stoin: failed to install Windows low-level keyboard hook\n", stderr);
        platform_shutdown();
        return false;
    }

    g_windows.watcher_timer_id = SetTimer(NULL, 0, WINDOWS_FILE_WATCH_POLL_MS, NULL);
    g_windows.running = true;
    return true;
}

bool platform_init_listen_only(Handle_Input_Fn handler, void *userdata)
{
    return platform_init(handler, userdata);
}

void platform_run(void)
{
    g_windows.running = true;
    while (g_windows.running) {
        MSG message;
        const BOOL result = GetMessageA(&message, NULL, 0, 0);
        if (result <= 0) {
            break;
        }
        if (message.message == WM_TIMER && message.wParam == g_windows.watcher_timer_id) {
            platform_file_watcher_poll();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

void platform_poll_input_events(void)
{
    if (!g_windows.running) {
        return;
    }

    MSG message;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_TIMER && message.wParam == g_windows.watcher_timer_id) {
            platform_file_watcher_poll();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

void platform_shutdown(void)
{
    g_windows.running = false;
    platform_file_watcher_stop();

    if (g_windows.watcher_timer_id != 0) {
        KillTimer(NULL, g_windows.watcher_timer_id);
        g_windows.watcher_timer_id = 0;
    }

    if (g_windows.keyboard_hook != NULL) {
        UnhookWindowsHookEx(g_windows.keyboard_hook);
        g_windows.keyboard_hook = NULL;
    }

    memset(g_windows.down_vks, 0, sizeof(g_windows.down_vks));
    g_windows.handler = NULL;
    g_windows.userdata = NULL;
    g_windows.shift_down = false;
    g_windows.control_down = false;
    g_windows.option_down = false;
    g_windows.command_down = false;
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

    for (size_t i = 0; i < *path_count; ++i) {
        if (strcmp(out_paths[i], path) == 0) {
            return false;
        }
    }

    const int written = snprintf(out_paths[*path_count], PLATFORM_SERIAL_PATH_MAX, "%s", path);
    if (written <= 0 || (size_t)written >= PLATFORM_SERIAL_PATH_MAX) {
        return false;
    }

    ++*path_count;
    return true;
}

static bool copy_registry_string_property(
    HDEVINFO device_info,
    SP_DEVINFO_DATA *device_data,
    DWORD property,
    char *out_value,
    size_t out_size
)
{
    if (out_value == NULL || out_size == 0) {
        return false;
    }
    out_value[0] = '\0';

    DWORD type = 0;
    DWORD required_size = 0;
    if (!SetupDiGetDeviceRegistryPropertyA(
            device_info,
            device_data,
            property,
            &type,
            (PBYTE)out_value,
            (DWORD)out_size,
            &required_size)
        || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out_value[0] = '\0';
        return false;
    }
    out_value[out_size - 1] = '\0';
    return true;
}

static bool copy_port_name_from_device_registry(
    HDEVINFO device_info,
    SP_DEVINFO_DATA *device_data,
    char *out_name,
    size_t out_size
)
{
    if (out_name == NULL || out_size == 0) {
        return false;
    }
    out_name[0] = '\0';

    HKEY key = SetupDiOpenDevRegKey(
        device_info,
        device_data,
        DICS_FLAG_GLOBAL,
        0,
        DIREG_DEV,
        KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD type = 0;
    DWORD size = (DWORD)out_size;
    const LONG result = RegQueryValueExA(key, "PortName", NULL, &type, (LPBYTE)out_name, &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out_name[0] = '\0';
        return false;
    }
    out_name[out_size - 1] = '\0';
    return true;
}

static bool windows_ascii_contains_ci(const char *text, const char *needle)
{
    if (text == NULL || needle == NULL || *needle == '\0') {
        return false;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        const char *a = p;
        const char *b = needle;
        while (*a != '\0'
            && *b != '\0'
            && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return true;
        }
    }
    return false;
}

static unsigned int serial_port_number_from_name(const char *name)
{
    if (name == NULL
        || (name[0] != 'C' && name[0] != 'c')
        || (name[1] != 'O' && name[1] != 'o')
        || (name[2] != 'M' && name[2] != 'm')
        || !isdigit((unsigned char)name[3])) {
        return 0;
    }

    char *end = NULL;
    const unsigned long parsed = strtoul(name + 3, &end, 10);
    if (end == name + 3 || *end != '\0' || parsed == 0 || parsed > 256) {
        return 0;
    }
    return (unsigned int)parsed;
}

static int windows_serial_metadata_score(const Windows_Serial_Candidate *candidate)
{
    if (candidate == NULL) {
        return 0;
    }

    int score = 0;
    const char *fields[] = {
        candidate->friendly_name,
        candidate->description,
        candidate->manufacturer,
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        const char *field = fields[i];
        if (windows_ascii_contains_ci(field, "usb serial port")) {
            score += 120;
        }
        if (windows_ascii_contains_ci(field, "usb serial")) {
            score += 80;
        }
        if (windows_ascii_contains_ci(field, "ftdi")) {
            score += 70;
        }
        if (windows_ascii_contains_ci(field, "usb")) {
            score += 30;
        }
        if (windows_ascii_contains_ci(field, "intel")
            || windows_ascii_contains_ci(field, "active management")
            || windows_ascii_contains_ci(field, "amt")) {
            score -= 100;
        }
    }
    return score;
}

static Windows_Serial_Candidate *find_serial_candidate(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t candidate_count,
    const char *name
)
{
    for (size_t i = 0; i < candidate_count; ++i) {
        if (strcmp(candidates[i].name, name) == 0) {
            return &candidates[i];
        }
    }
    return NULL;
}

static Windows_Serial_Candidate *add_or_find_serial_candidate(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t *candidate_count,
    const char *name
)
{
    if (candidate_count == NULL || name == NULL) {
        return NULL;
    }

    Windows_Serial_Candidate *existing = find_serial_candidate(candidates, *candidate_count, name);
    if (existing != NULL) {
        return existing;
    }
    if (*candidate_count >= WINDOWS_SERIAL_MAX_CANDIDATES) {
        return NULL;
    }

    Windows_Serial_Candidate *candidate = &candidates[(*candidate_count)++];
    memset(candidate, 0, sizeof(*candidate));
    const int name_written = snprintf(candidate->name, sizeof(candidate->name), "%s", name);
    const int path_written = snprintf(candidate->path, sizeof(candidate->path), "\\\\.\\%s", name);
    if (name_written <= 0
        || (size_t)name_written >= sizeof(candidate->name)
        || path_written <= 0
        || (size_t)path_written >= sizeof(candidate->path)) {
        --*candidate_count;
        return NULL;
    }
    candidate->number = serial_port_number_from_name(name);
    candidate->exists = true;
    return candidate;
}

static void add_setupapi_serial_candidates(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t *candidate_count
)
{
    GUID ports_guid = {0};
    DWORD guid_count = 0;
    if (!SetupDiClassGuidsFromNameA("Ports", &ports_guid, 1, &guid_count) || guid_count == 0) {
        return;
    }

    HDEVINFO device_info = SetupDiGetClassDevsA(&ports_guid, NULL, NULL, DIGCF_PRESENT);
    if (device_info == INVALID_HANDLE_VALUE) {
        return;
    }

    for (DWORD index = 0; ; ++index) {
        SP_DEVINFO_DATA device_data;
        memset(&device_data, 0, sizeof(device_data));
        device_data.cbSize = sizeof(device_data);
        if (!SetupDiEnumDeviceInfo(device_info, index, &device_data)) {
            break;
        }

        char port_name[16] = {0};
        if (!copy_port_name_from_device_registry(device_info, &device_data, port_name, sizeof(port_name))
            || serial_port_number_from_name(port_name) == 0) {
            continue;
        }

        Windows_Serial_Candidate *candidate =
            add_or_find_serial_candidate(candidates, candidate_count, port_name);
        if (candidate == NULL) {
            continue;
        }

        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_FRIENDLYNAME,
            candidate->friendly_name,
            sizeof(candidate->friendly_name));
        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_DEVICEDESC,
            candidate->description,
            sizeof(candidate->description));
        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_MFG,
            candidate->manufacturer,
            sizeof(candidate->manufacturer));
        candidate->score = windows_serial_metadata_score(candidate);
    }

    SetupDiDestroyDeviceInfoList(device_info);
}

static int compare_serial_candidates(const void *a, const void *b)
{
    const Windows_Serial_Candidate *left = a;
    const Windows_Serial_Candidate *right = b;
    if (left->score != right->score) {
        return right->score - left->score;
    }
    if (left->number < right->number) {
        return -1;
    }
    if (left->number > right->number) {
        return 1;
    }
    return strcmp(left->name, right->name);
}

size_t platform_find_serial_devices(char out_paths[][PLATFORM_SERIAL_PATH_MAX], size_t max_paths)
{
    if (out_paths == NULL || max_paths == 0) {
        return 0;
    }

    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES];
    memset(candidates, 0, sizeof(candidates));
    size_t candidate_count = 0;
    add_setupapi_serial_candidates(candidates, &candidate_count);

    for (unsigned int i = 1; i <= 256; ++i) {
        char name[16] = {0};
        char target[512] = {0};
        snprintf(name, sizeof(name), "COM%u", i);
        if (QueryDosDeviceA(name, target, sizeof(target)) != 0) {
            (void)add_or_find_serial_candidate(candidates, &candidate_count, name);
        }
    }

    qsort(candidates, candidate_count, sizeof(candidates[0]), compare_serial_candidates);

    size_t path_count = 0;
    for (size_t i = 0; i < candidate_count && path_count < max_paths; ++i) {
        if (candidates[i].exists) {
            (void)add_serial_path(out_paths, max_paths, &path_count, candidates[i].path);
        }
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

static bool normalize_serial_path(const char *port_path, char *out_path, size_t out_size)
{
    if (port_path == NULL || out_path == NULL || out_size == 0) {
        return false;
    }

    if (strncmp(port_path, "\\\\.\\", 4) == 0) {
        const int written = snprintf(out_path, out_size, "%s", port_path);
        return written > 0 && (size_t)written < out_size;
    }

    if ((port_path[0] == 'C' || port_path[0] == 'c')
        && (port_path[1] == 'O' || port_path[1] == 'o')
        && (port_path[2] == 'M' || port_path[2] == 'm')) {
        const int written = snprintf(out_path, out_size, "\\\\.\\%s", port_path);
        return written > 0 && (size_t)written < out_size;
    }

    const int written = snprintf(out_path, out_size, "%s", port_path);
    return written > 0 && (size_t)written < out_size;
}

static bool configure_comm_timeouts(HANDLE handle, DWORD timeout_ms)
{
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    timeouts.WriteTotalTimeoutConstant = timeout_ms;
    return SetCommTimeouts(handle, &timeouts);
}

bool platform_serial_open(Platform_Serial_Port **out_port, const char *port_path, int baud_rate)
{
    if (out_port == NULL) {
        return false;
    }
    *out_port = NULL;

    char normalized_path[PLATFORM_SERIAL_PATH_MAX] = {0};
    if (!normalize_serial_path(port_path, normalized_path, sizeof(normalized_path))) {
        errno = port_path == NULL ? ENODEV : ENAMETOOLONG;
        return false;
    }

    HANDLE handle = CreateFileA(
        normalized_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE) {
        errno = ENODEV;
        return false;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        errno = EIO;
        return false;
    }

    dcb.BaudRate = (DWORD)baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(handle, &dcb)
        || !SetupComm(handle, 4096, 4096)
        || !configure_comm_timeouts(handle, 100)
        || !PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        CloseHandle(handle);
        errno = EIO;
        return false;
    }

    Platform_Serial_Port *port = calloc(1, sizeof(*port));
    if (port == NULL) {
        CloseHandle(handle);
        errno = ENOMEM;
        return false;
    }

    port->handle = handle;
    snprintf(port->port_path, sizeof(port->port_path), "%s", normalized_path);
    *out_port = port;
    return true;
}

void platform_serial_close(Platform_Serial_Port *port)
{
    if (port == NULL) {
        return;
    }
    if (port->handle != NULL && port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
        port->handle = INVALID_HANDLE_VALUE;
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

void platform_serial_flush(Platform_Serial_Port *port)
{
    if (port == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return;
    }
    PurgeComm(port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

Platform_Serial_Read_Result platform_serial_read_byte(
    Platform_Serial_Port *port,
    uint8_t *out_byte,
    unsigned int timeout_ms
)
{
    if (port == NULL || out_byte == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        if (port != NULL) {
            port->had_error = true;
        }
        errno = EBADF;
        return PLATFORM_SERIAL_READ_ERROR;
    }

    if (!configure_comm_timeouts(port->handle, timeout_ms)) {
        port->had_error = true;
        return PLATFORM_SERIAL_READ_ERROR;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(port->handle, out_byte, 1, &bytes_read, NULL)) {
        port->had_error = true;
        return PLATFORM_SERIAL_READ_ERROR;
    }
    if (bytes_read == 1) {
        return PLATFORM_SERIAL_READ_BYTE;
    }
    return PLATFORM_SERIAL_READ_NONE;
}

bool platform_serial_write_all(
    Platform_Serial_Port *port,
    const uint8_t *bytes,
    size_t byte_count,
    unsigned int timeout_ms
)
{
    if (port == NULL || bytes == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        if (port != NULL) {
            port->had_error = true;
        }
        errno = EBADF;
        return false;
    }

    if (!configure_comm_timeouts(port->handle, timeout_ms)) {
        port->had_error = true;
        return false;
    }

    size_t written_count = 0;
    while (written_count < byte_count) {
        const size_t remaining = byte_count - written_count;
        const DWORD request_count = remaining > UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
        DWORD bytes_written = 0;
        if (!WriteFile(port->handle, bytes + written_count, request_count, &bytes_written, NULL)) {
            port->had_error = true;
            return false;
        }
        if (bytes_written == 0) {
            errno = ETIMEDOUT;
            return false;
        }
        written_count += bytes_written;
    }

    return true;
}
