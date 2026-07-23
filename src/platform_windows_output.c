#include "platform_windows_internal.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum Windows_Combo_Modifier {
    WINDOWS_COMBO_SHIFT = 1 << 0,
    WINDOWS_COMBO_CONTROL = 1 << 1,
    WINDOWS_COMBO_ALT = 1 << 2,
    WINDOWS_COMBO_SUPER = 1 << 3,
} Windows_Combo_Modifier;

typedef struct Windows_Output_State {
    uint64_t translation_timing_start_ns;
    uint64_t translation_timing_sequence;
    bool translation_timing_enabled;
    bool translation_timing_active;
} Windows_Output_State;

static Windows_Output_State g_windows;

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
