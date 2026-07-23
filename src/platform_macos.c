#include "platform_macos_internal.h"

#include <ctype.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAC_SESSION_SCREEN_IS_LOCKED_KEY CFSTR("CGSSessionScreenIsLocked")
#define MAC_SESSION_AGENT_SCREEN_IS_LOCKED "com.apple.sessionagent.screenIsLocked"

Mac_State g_macos;

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
    } else if (key_name_equals(name, "f1")) {
        *out_keycode = 122;
    } else if (key_name_equals(name, "f2")) {
        *out_keycode = 120;
    } else if (key_name_equals(name, "f3")) {
        *out_keycode = 99;
    } else if (key_name_equals(name, "f4")) {
        *out_keycode = 118;
    } else if (key_name_equals(name, "f5")) {
        *out_keycode = 96;
    } else if (key_name_equals(name, "f6")) {
        *out_keycode = 97;
    } else if (key_name_equals(name, "f7")) {
        *out_keycode = 98;
    } else if (key_name_equals(name, "f8")) {
        *out_keycode = 100;
    } else if (key_name_equals(name, "f9")) {
        *out_keycode = 101;
    } else if (key_name_equals(name, "f10")) {
        *out_keycode = 109;
    } else if (key_name_equals(name, "f11")) {
        *out_keycode = 103;
    } else if (key_name_equals(name, "f12")) {
        *out_keycode = 111;
    } else if (key_name_equals(name, "f13")) {
        *out_keycode = 105;
    } else if (key_name_equals(name, "f14")) {
        *out_keycode = 107;
    } else if (key_name_equals(name, "f15")) {
        *out_keycode = 113;
    } else if (key_name_equals(name, "f16")) {
        *out_keycode = 106;
    } else if (key_name_equals(name, "f17")) {
        *out_keycode = 64;
    } else if (key_name_equals(name, "f18")) {
        *out_keycode = 79;
    } else if (key_name_equals(name, "f19")) {
        *out_keycode = 80;
    } else if (key_name_equals(name, "f20")) {
        *out_keycode = 90;
    } else {
        return false;
    }

    return true;
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

void platform_translation_timing_set_enabled(bool enabled)
{
    g_macos.translation_timing_enabled = enabled;
    if (!enabled) {
        g_macos.translation_timing_active = false;
        g_macos.translation_timing_start_ns = 0;
    }
}

void platform_translation_timing_begin(uint64_t start_ns)
{
    if (!g_macos.translation_timing_enabled || start_ns == 0) {
        return;
    }

    g_macos.translation_timing_start_ns = start_ns;
    g_macos.translation_timing_active = true;
}

void platform_translation_timing_cancel(void)
{
    g_macos.translation_timing_active = false;
    g_macos.translation_timing_start_ns = 0;
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

    while (nanosleep(&requested, &requested) != 0) {
    }
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

    if (macos_event_was_generated_by_stoin(event)) {
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

    if (g_macos.handler != NULL && g_macos.handler(&input, g_macos.userdata) && !g_macos.tap_listen_only) {
        return NULL;
    }

    return event;
}

static void macos_clear_keyboard_tap(void)
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

    g_macos.run_loop = NULL;
    g_macos.tap_listen_only = false;
}

static bool platform_init_tap(Handle_Input_Fn handler, void *userdata, bool listen_only)
{
    g_macos.handler = handler;
    g_macos.userdata = userdata;
    g_macos.tap_listen_only = listen_only;

    if (!platform_output_init()) {
        return false;
    }

    const CGEventMask keyboard_events = CGEventMaskBit(kCGEventKeyDown)
        | CGEventMaskBit(kCGEventKeyUp)
        | CGEventMaskBit(kCGEventFlagsChanged);
    g_macos.tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        listen_only ? kCGEventTapOptionListenOnly : kCGEventTapOptionDefault,
        keyboard_events,
        keyboard_tap_callback,
        NULL
    );

    if (g_macos.tap == NULL) {
        fputs("stoin: failed to create keyboard event tap\n", stderr);
        fputs("stoin: confirm Accessibility permission, then restart the executable\n", stderr);
        macos_clear_keyboard_tap();
        return false;
    }

    g_macos.run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_macos.tap, 0);
    if (g_macos.run_loop_source == NULL) {
        fputs("stoin: failed to create run loop source for event tap\n", stderr);
        macos_clear_keyboard_tap();
        return false;
    }

    g_macos.run_loop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(g_macos.run_loop, g_macos.run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(g_macos.tap, true);

    return true;
}

bool platform_init(Handle_Input_Fn handler, void *userdata)
{
    return platform_init_tap(handler, userdata, false);
}

static bool macos_init_tap_thread_sync(void)
{
    if (g_macos.tap_thread_sync_initialized) {
        return true;
    }

    int error = pthread_mutex_init(&g_macos.tap_thread_mutex, NULL);
    if (error != 0) {
        fprintf(stderr, "stoin: failed to initialize keyboard listener mutex (%s)\n", strerror(error));
        return false;
    }

    error = pthread_cond_init(&g_macos.tap_thread_condition, NULL);
    if (error != 0) {
        fprintf(stderr, "stoin: failed to initialize keyboard listener condition (%s)\n", strerror(error));
        pthread_mutex_destroy(&g_macos.tap_thread_mutex);
        return false;
    }

    g_macos.tap_thread_sync_initialized = true;
    return true;
}

static void macos_signal_tap_thread_ready(bool ok)
{
    pthread_mutex_lock(&g_macos.tap_thread_mutex);
    g_macos.tap_thread_ok = ok;
    g_macos.tap_thread_ready = true;
    pthread_cond_signal(&g_macos.tap_thread_condition);
    pthread_mutex_unlock(&g_macos.tap_thread_mutex);
}

static void *macos_listen_only_tap_thread_main(void *userdata)
{
    (void)userdata;

    const bool ok = platform_init_tap(g_macos.handler, g_macos.userdata, true);
    macos_signal_tap_thread_ready(ok);
    if (ok) {
        CFRunLoopRun();
        macos_clear_keyboard_tap();
    }

    return NULL;
}

bool platform_init_listen_only(Handle_Input_Fn handler, void *userdata)
{
    if (!macos_init_tap_thread_sync()) {
        return false;
    }
    if (!platform_output_init()) {
        return false;
    }

    g_macos.handler = handler;
    g_macos.userdata = userdata;
    g_macos.tap_listen_only = true;

    pthread_mutex_lock(&g_macos.tap_thread_mutex);
    g_macos.tap_thread_ready = false;
    g_macos.tap_thread_ok = false;
    pthread_mutex_unlock(&g_macos.tap_thread_mutex);

    const int error = pthread_create(&g_macos.tap_thread, NULL, macos_listen_only_tap_thread_main, NULL);
    if (error != 0) {
        fprintf(stderr, "stoin: failed to start keyboard listener thread (%s)\n", strerror(error));
        g_macos.handler = NULL;
        g_macos.userdata = NULL;
        g_macos.tap_listen_only = false;
        return false;
    }
    g_macos.tap_thread_started = true;

    pthread_mutex_lock(&g_macos.tap_thread_mutex);
    while (!g_macos.tap_thread_ready) {
        pthread_cond_wait(&g_macos.tap_thread_condition, &g_macos.tap_thread_mutex);
    }
    const bool ok = g_macos.tap_thread_ok;
    pthread_mutex_unlock(&g_macos.tap_thread_mutex);

    if (!ok) {
        pthread_join(g_macos.tap_thread, NULL);
        g_macos.tap_thread_started = false;
        return false;
    }

    return true;
}

void platform_run(void)
{
    CFRunLoopRun();
}

void platform_poll_input_events(void)
{
    if (g_macos.tap_thread_started) {
        return;
    }
    if (g_macos.run_loop == NULL) {
        return;
    }

    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true) == kCFRunLoopRunHandledSource) {
    }
}

void platform_shutdown(void)
{
    platform_pedals_shutdown();
    platform_file_watcher_stop();

    if (g_macos.tap_thread_started) {
        if (g_macos.run_loop != NULL) {
            CFRunLoopStop(g_macos.run_loop);
        }
        pthread_join(g_macos.tap_thread, NULL);
        g_macos.tap_thread_started = false;
    } else {
        macos_clear_keyboard_tap();
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

    g_macos.handler = NULL;
    g_macos.userdata = NULL;
}
