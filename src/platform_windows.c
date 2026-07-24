#include "platform_windows_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WINDOWS_FILE_WATCH_POLL_MS 250

typedef struct Windows_Key_Map {
    const char *name;
    uint16_t logical_keycode;
    UINT vk;
    char printable;
} Windows_Key_Map;

typedef struct Windows_State {
    HHOOK keyboard_hook;
    UINT_PTR watcher_timer_id;
    Handle_Input_Fn handler;
    void *userdata;
    uint64_t performance_frequency;
    bool down_vks[256];
    bool running;
    bool shift_down;
    bool control_down;
    bool option_down;
    bool command_down;
} Windows_State;

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

bool windows_vk_from_logical(uint16_t logical_keycode, UINT *out_vk)
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
