#include "platform_macos_internal.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

#define MAC_BACKSPACE_KEYCODE 51

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

void macos_mark_generated_event(CGEventRef event)
{
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, STOIN_GENERATED_EVENT_USER_DATA);
}

bool macos_event_was_generated_by_stoin(CGEventRef event)
{
    return CGEventGetIntegerValueField(event, kCGEventSourceUserData) == STOIN_GENERATED_EVENT_USER_DATA;
}

static void report_translation_timing_before_cgevent_post(const char *operation)
{
    if (!g_macos.translation_timing_enabled || !g_macos.translation_timing_active) {
        return;
    }

    const uint64_t now_ns = platform_monotonic_ns();
    const uint64_t start_ns = g_macos.translation_timing_start_ns;
    const uint64_t elapsed_ns = now_ns >= start_ns ? now_ns - start_ns : 0;
    g_macos.translation_timing_active = false;
    g_macos.translation_timing_start_ns = 0;
    ++g_macos.translation_timing_sequence;

    fprintf(stderr,
        "stoin: translation latency #%" PRIu64 " before %s CGEventPost: %.3f ms (%.1f us)\n",
        g_macos.translation_timing_sequence,
        operation != NULL ? operation : "first",
        (double)elapsed_ns / 1000000.0,
        (double)elapsed_ns / 1000.0);
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

static bool post_keyboard_event_pair(CGKeyCode keycode)
{
    CGEventRef key_down = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, false);
    bool ok = false;

    if (key_down != NULL && key_up != NULL) {
        macos_mark_generated_event(key_down);
        macos_mark_generated_event(key_up);
        CGEventSetFlags(key_down, 0);
        CGEventSetFlags(key_up, 0);
        report_translation_timing_before_cgevent_post("key");
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

static bool post_keyboard_event_pair_with_flags(CGKeyCode keycode, CGEventFlags flags)
{
    CGEventRef key_down = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(g_macos.output_source, keycode, false);
    bool ok = false;

    if (key_down != NULL && key_up != NULL) {
        macos_mark_generated_event(key_down);
        macos_mark_generated_event(key_up);
        CGEventSetFlags(key_down, flags);
        CGEventSetFlags(key_up, flags);
        report_translation_timing_before_cgevent_post("key-combo");
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

static bool combo_modifier_flag(const char *start, size_t length, CGEventFlags *out_flag)
{
    if (combo_token_equals(start, length, "shift")
        || combo_token_equals(start, length, "shift_l")
        || combo_token_equals(start, length, "shift_r")) {
        *out_flag = kCGEventFlagMaskShift;
        return true;
    }
    if (combo_token_equals(start, length, "control")
        || combo_token_equals(start, length, "control_l")
        || combo_token_equals(start, length, "control_r")
        || combo_token_equals(start, length, "ctrl")) {
        *out_flag = kCGEventFlagMaskControl;
        return true;
    }
    if (combo_token_equals(start, length, "alt")
        || combo_token_equals(start, length, "alt_l")
        || combo_token_equals(start, length, "alt_r")
        || combo_token_equals(start, length, "option")
        || combo_token_equals(start, length, "option_l")
        || combo_token_equals(start, length, "option_r")) {
        *out_flag = kCGEventFlagMaskAlternate;
        return true;
    }
    if (combo_token_equals(start, length, "command")
        || combo_token_equals(start, length, "super")
        || combo_token_equals(start, length, "super_l")
        || combo_token_equals(start, length, "super_r")
        || combo_token_equals(start, length, "windows")) {
        *out_flag = kCGEventFlagMaskCommand;
        return true;
    }
    return false;
}

static bool combo_keycode_from_token(const char *start, size_t length, CGKeyCode *out_keycode)
{
    if (length == 1) {
        char key_name[2] = { (char)tolower((unsigned char)start[0]), '\0' };
        uint16_t keycode = 0;
        if (platform_keycode_from_name(key_name, &keycode)) {
            *out_keycode = (CGKeyCode)keycode;
            return true;
        }
    }

    if (combo_token_equals(start, length, "home")) {
        *out_keycode = 115;
    } else if (combo_token_equals(start, length, "end")) {
        *out_keycode = 119;
    } else if (combo_token_equals(start, length, "page_up") || combo_token_equals(start, length, "pageup")) {
        *out_keycode = 116;
    } else if (combo_token_equals(start, length, "page_down") || combo_token_equals(start, length, "pagedown")) {
        *out_keycode = 121;
    } else if (combo_token_equals(start, length, "left")) {
        *out_keycode = 123;
    } else if (combo_token_equals(start, length, "right")) {
        *out_keycode = 124;
    } else if (combo_token_equals(start, length, "down")) {
        *out_keycode = 125;
    } else if (combo_token_equals(start, length, "up")) {
        *out_keycode = 126;
    } else if (combo_token_equals(start, length, "tab")) {
        *out_keycode = 48;
    } else if (combo_token_equals(start, length, "return") || combo_token_equals(start, length, "enter")) {
        *out_keycode = 36;
    } else if (combo_token_equals(start, length, "backspace")) {
        *out_keycode = 51;
    } else if (combo_token_equals(start, length, "delete")) {
        *out_keycode = 117;
    } else if (combo_token_equals(start, length, "escape") || combo_token_equals(start, length, "esc")) {
        *out_keycode = 53;
    } else if (length >= 2 && (start[0] == 'F' || start[0] == 'f') && isdigit((unsigned char)start[1])) {
        char buffer[4] = {0};
        if (length >= sizeof(buffer)) {
            return false;
        }
        memcpy(buffer, start + 1, length - 1);
        const int function_key = atoi(buffer);
        switch (function_key) {
        case 1: *out_keycode = 122; break;
        case 2: *out_keycode = 120; break;
        case 3: *out_keycode = 99; break;
        case 4: *out_keycode = 118; break;
        case 5: *out_keycode = 96; break;
        case 6: *out_keycode = 97; break;
        case 7: *out_keycode = 98; break;
        case 8: *out_keycode = 100; break;
        case 9: *out_keycode = 101; break;
        case 10: *out_keycode = 109; break;
        case 11: *out_keycode = 103; break;
        case 12: *out_keycode = 111; break;
        default: return false;
        }
    } else {
        return false;
    }

    return true;
}

static bool parse_combo_expression(const char **cursor, CGEventFlags *flags, CGKeyCode *out_keycode)
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
        CGEventFlags flag = 0;
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
    CGEventFlags flags = 0;
    CGKeyCode keycode = 0;
    if (!parse_combo_expression(&p, &flags, &keycode)) {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }
    p = skip_combo_ws(p);
    if (*p != '\0') {
        fprintf(stderr, "stoin: unsupported key combo '%s'\n", combo);
        return false;
    }

    return post_keyboard_event_pair_with_flags(keycode, flags);
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
        macos_mark_generated_event(key_down);
        macos_mark_generated_event(key_up);
        CGEventSetFlags(key_down, 0);
        CGEventSetFlags(key_up, 0);
        CGEventKeyboardSetUnicodeString(key_down, (UniCharCount)length, utf16);
        CGEventKeyboardSetUnicodeString(key_up, (UniCharCount)length, utf16);
        report_translation_timing_before_cgevent_post("text");
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
