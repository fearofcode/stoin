#+build windows
package stoin

import "core:fmt"
import win "core:sys/windows"

WINDOWS_STOIN_EXTRA_INFO :: win.ULONG_PTR(0x73746f696e)
WINDOWS_KEYEVENTF_KEYUP :: win.DWORD(0x0002)
WINDOWS_KEYEVENTF_UNICODE :: win.DWORD(0x0004)
WINDOWS_COMBO_SHIFT :: 1 << 0
WINDOWS_COMBO_CONTROL :: 1 << 1
WINDOWS_COMBO_ALT :: 1 << 2
WINDOWS_COMBO_SUPER :: 1 << 3

windows_performance_frequency: u64
windows_translation_timing_enabled: bool
windows_translation_timing_active: bool
windows_translation_timing_start_ns: u64
windows_translation_timing_sequence: u64

windows_monotonic_ns :: proc() -> u64 {
	if windows_performance_frequency == 0 {
		frequency: win.LARGE_INTEGER
		if win.QueryPerformanceFrequency(&frequency) == win.FALSE || i64(frequency) <= 0 {
			return 0
		}
		windows_performance_frequency = u64(frequency)
	}

	now: win.LARGE_INTEGER
	if win.QueryPerformanceCounter(&now) == win.FALSE {
		return 0
	}
	return (u64(now) * 1_000_000_000) / windows_performance_frequency
}

windows_sleep_ms :: proc(milliseconds: uint) {
	win.Sleep(win.DWORD(milliseconds))
}

windows_translation_timing_set_enabled :: proc(enabled: bool) {
	windows_translation_timing_enabled = enabled
	if !enabled {
		windows_translation_timing_active = false
		windows_translation_timing_start_ns = 0
	}
}

windows_translation_timing_begin :: proc(start_ns: u64, userdata: rawptr) {
	_ = userdata
	if !windows_translation_timing_enabled || start_ns == 0 {
		return
	}
	windows_translation_timing_start_ns = start_ns
	windows_translation_timing_active = true
}

windows_translation_timing_cancel :: proc(userdata: rawptr) {
	_ = userdata
	windows_translation_timing_active = false
	windows_translation_timing_start_ns = 0
}

windows_report_translation_timing_before_output :: proc(operation: string) {
	if !windows_translation_timing_enabled || !windows_translation_timing_active {
		return
	}
	now_ns := windows_monotonic_ns()
	start_ns := windows_translation_timing_start_ns
	elapsed_ns: u64
	if now_ns >= start_ns {
		elapsed_ns = now_ns - start_ns
	}
	windows_translation_timing_active = false
	windows_translation_timing_start_ns = 0
	windows_translation_timing_sequence += 1

	label := operation
	if len(label) == 0 {
		label = "first"
	}
	fmt.eprintf(
		"stoin: translation latency #%d before %s SendInput: %.3f ms (%.1f us)\n",
		windows_translation_timing_sequence,
		label,
		f64(elapsed_ns) / 1_000_000.0,
		f64(elapsed_ns) / 1_000.0,
	)
}

windows_output_init :: proc() -> bool {
	return true
}

windows_output_shutdown :: proc() {}

windows_send_inputs :: proc(inputs: []win.INPUT) -> bool {
	if len(inputs) == 0 {
		return false
	}
	return win.SendInput(win.UINT(len(inputs)), raw_data(inputs), win.INT(size_of(win.INPUT))) == win.UINT(len(inputs))
}

windows_keyboard_input_vk :: proc(vk: win.WORD, flags: win.DWORD) -> win.INPUT {
	input: win.INPUT
	input.type = .KEYBOARD
	input.ki.wVk = vk
	input.ki.dwFlags = flags
	input.ki.dwExtraInfo = WINDOWS_STOIN_EXTRA_INFO
	return input
}

windows_keyboard_input_unicode :: proc(code_unit: u16, flags: win.DWORD) -> win.INPUT {
	input: win.INPUT
	input.type = .KEYBOARD
	input.ki.wScan = win.WORD(code_unit)
	input.ki.dwFlags = WINDOWS_KEYEVENTF_UNICODE | flags
	input.ki.dwExtraInfo = WINDOWS_STOIN_EXTRA_INFO
	return input
}

windows_tap_vk :: proc(vk: win.WORD) -> bool {
	inputs := [?]win.INPUT {
		windows_keyboard_input_vk(vk, 0),
		windows_keyboard_input_vk(vk, WINDOWS_KEYEVENTF_KEYUP),
	}
	return windows_send_inputs(inputs[:])
}

windows_logical_keycode_to_vk :: proc(logical: u16) -> (win.WORD, bool) {
	switch logical {
	case 0: return win.WORD(win.VK_A), true
	case 1: return win.WORD(win.VK_S), true
	case 2: return win.WORD(win.VK_D), true
	case 3: return win.WORD(win.VK_F), true
	case 4: return win.WORD(win.VK_H), true
	case 5: return win.WORD(win.VK_G), true
	case 6: return win.WORD(win.VK_Z), true
	case 7: return win.WORD(win.VK_X), true
	case 8: return win.WORD(win.VK_C), true
	case 9: return win.WORD(win.VK_V), true
	case 11: return win.WORD(win.VK_B), true
	case 12: return win.WORD(win.VK_Q), true
	case 13: return win.WORD(win.VK_W), true
	case 14: return win.WORD(win.VK_E), true
	case 15: return win.WORD(win.VK_R), true
	case 16: return win.WORD(win.VK_Y), true
	case 17: return win.WORD(win.VK_T), true
	case 18: return win.WORD(win.VK_1), true
	case 19: return win.WORD(win.VK_2), true
	case 20: return win.WORD(win.VK_3), true
	case 21: return win.WORD(win.VK_4), true
	case 22: return win.WORD(win.VK_6), true
	case 23: return win.WORD(win.VK_5), true
	case 25: return win.WORD(win.VK_9), true
	case 26: return win.WORD(win.VK_7), true
	case 28: return win.WORD(win.VK_8), true
	case 29: return win.WORD(win.VK_0), true
	case 30: return win.WORD(win.VK_OEM_6), true
	case 31: return win.WORD(win.VK_O), true
	case 32: return win.WORD(win.VK_U), true
	case 33: return win.WORD(win.VK_OEM_4), true
	case 34: return win.WORD(win.VK_I), true
	case 35: return win.WORD(win.VK_P), true
	case 36: return win.WORD(win.VK_RETURN), true
	case 37: return win.WORD(win.VK_L), true
	case 38: return win.WORD(win.VK_J), true
	case 39: return win.WORD(win.VK_OEM_7), true
	case 40: return win.WORD(win.VK_K), true
	case 41: return win.WORD(win.VK_OEM_1), true
	case 42: return win.WORD(win.VK_OEM_5), true
	case 43: return win.WORD(win.VK_OEM_COMMA), true
	case 44: return win.WORD(win.VK_OEM_2), true
	case 45: return win.WORD(win.VK_N), true
	case 46: return win.WORD(win.VK_M), true
	case 47: return win.WORD(win.VK_OEM_PERIOD), true
	case 48: return win.WORD(win.VK_TAB), true
	case 49: return win.WORD(win.VK_SPACE), true
	case 50: return win.WORD(win.VK_OEM_3), true
	case 51: return win.WORD(win.VK_BACK), true
	case 53: return win.WORD(win.VK_ESCAPE), true
	case 54: return win.WORD(win.VK_RWIN), true
	case 55: return win.WORD(win.VK_LWIN), true
	case 56: return win.WORD(win.VK_LSHIFT), true
	case 58: return win.WORD(win.VK_LMENU), true
	case 59: return win.WORD(win.VK_LCONTROL), true
	case 60: return win.WORD(win.VK_RSHIFT), true
	case 61: return win.WORD(win.VK_RMENU), true
	case 62: return win.WORD(win.VK_RCONTROL), true
	case 64: return win.WORD(win.VK_F17), true
	case 79: return win.WORD(win.VK_F18), true
	case 80: return win.WORD(win.VK_F19), true
	case 90: return win.WORD(win.VK_F20), true
	case 105: return win.WORD(win.VK_F13), true
	case 106: return win.WORD(win.VK_F16), true
	case 107: return win.WORD(win.VK_F14), true
	case 113: return win.WORD(win.VK_F15), true
	}
	return 0, false
}

windows_combo_token_equals :: proc(combo: string, start, end: int, expected: string) -> bool {
	return keymap_ascii_equal_ignore_case(combo[start:end], expected)
}

windows_combo_skip_ws :: proc(combo: string, cursor: int) -> int {
	index := cursor
	for index < len(combo) && keymap_is_space(combo[index]) {
		index += 1
	}
	return index
}

windows_combo_is_alphanumeric :: proc(c: byte) -> bool {
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9')
}

windows_combo_modifier_flag :: proc(combo: string, start, end: int) -> (int, bool) {
	if windows_combo_token_equals(combo, start, end, "shift") ||
	   windows_combo_token_equals(combo, start, end, "shift_l") ||
	   windows_combo_token_equals(combo, start, end, "shift_r") {
		return WINDOWS_COMBO_SHIFT, true
	}
	if windows_combo_token_equals(combo, start, end, "control") ||
	   windows_combo_token_equals(combo, start, end, "control_l") ||
	   windows_combo_token_equals(combo, start, end, "control_r") ||
	   windows_combo_token_equals(combo, start, end, "ctrl") {
		return WINDOWS_COMBO_CONTROL, true
	}
	if windows_combo_token_equals(combo, start, end, "alt") ||
	   windows_combo_token_equals(combo, start, end, "alt_l") ||
	   windows_combo_token_equals(combo, start, end, "alt_r") ||
	   windows_combo_token_equals(combo, start, end, "option") ||
	   windows_combo_token_equals(combo, start, end, "option_l") ||
	   windows_combo_token_equals(combo, start, end, "option_r") {
		return WINDOWS_COMBO_ALT, true
	}
	if windows_combo_token_equals(combo, start, end, "command") ||
	   windows_combo_token_equals(combo, start, end, "super") ||
	   windows_combo_token_equals(combo, start, end, "super_l") ||
	   windows_combo_token_equals(combo, start, end, "super_r") ||
	   windows_combo_token_equals(combo, start, end, "windows") {
		return WINDOWS_COMBO_SUPER, true
	}
	return 0, false
}

windows_vk_for_function_key :: proc(function_key: int) -> (win.WORD, bool) {
	if function_key < 1 || function_key > 24 {
		return 0, false
	}
	return win.WORD(win.VK_F1 + function_key - 1), true
}

windows_combo_keycode_from_token :: proc(combo: string, start, end: int) -> (win.WORD, bool) {
	if start >= end {
		return 0, false
	}
	if end - start == 1 {
		key_name := combo[start:end]
		logical, logical_ok := keycode_from_name(key_name)
		if logical_ok {
			return windows_logical_keycode_to_vk(logical)
		}
	}

	if windows_combo_token_equals(combo, start, end, "home") {
		return win.WORD(win.VK_HOME), true
	}
	if windows_combo_token_equals(combo, start, end, "end") {
		return win.WORD(win.VK_END), true
	}
	if windows_combo_token_equals(combo, start, end, "page_up") ||
	   windows_combo_token_equals(combo, start, end, "pageup") {
		return win.WORD(win.VK_PRIOR), true
	}
	if windows_combo_token_equals(combo, start, end, "page_down") ||
	   windows_combo_token_equals(combo, start, end, "pagedown") {
		return win.WORD(win.VK_NEXT), true
	}
	if windows_combo_token_equals(combo, start, end, "left") {
		return win.WORD(win.VK_LEFT), true
	}
	if windows_combo_token_equals(combo, start, end, "right") {
		return win.WORD(win.VK_RIGHT), true
	}
	if windows_combo_token_equals(combo, start, end, "down") {
		return win.WORD(win.VK_DOWN), true
	}
	if windows_combo_token_equals(combo, start, end, "up") {
		return win.WORD(win.VK_UP), true
	}
	if windows_combo_token_equals(combo, start, end, "tab") {
		return win.WORD(win.VK_TAB), true
	}
	if windows_combo_token_equals(combo, start, end, "return") ||
	   windows_combo_token_equals(combo, start, end, "enter") {
		return win.WORD(win.VK_RETURN), true
	}
	if windows_combo_token_equals(combo, start, end, "backspace") {
		return win.WORD(win.VK_BACK), true
	}
	if windows_combo_token_equals(combo, start, end, "delete") {
		return win.WORD(win.VK_DELETE), true
	}
	if windows_combo_token_equals(combo, start, end, "escape") ||
	   windows_combo_token_equals(combo, start, end, "esc") {
		return win.WORD(win.VK_ESCAPE), true
	}
	if end - start >= 2 && keymap_ascii_to_lower(combo[start]) == 'f' {
		value := 0
		for i := start + 1; i < end; i += 1 {
			if combo[i] < '0' || combo[i] > '9' {
				return 0, false
			}
			value = value * 10 + int(combo[i] - '0')
		}
		return windows_vk_for_function_key(value)
	}
	return 0, false
}

windows_parse_key_combination :: proc(combo: string, cursor: ^int, modifiers: ^int) -> (win.WORD, bool) {
	index := windows_combo_skip_ws(combo, cursor^)
	token_start := index
	for index < len(combo) && (windows_combo_is_alphanumeric(combo[index]) || combo[index] == '_') {
		index += 1
	}
	token_end := index
	if token_start == token_end {
		return 0, false
	}

	index = windows_combo_skip_ws(combo, index)
	if index < len(combo) && combo[index] == '(' {
		flag, flag_ok := windows_combo_modifier_flag(combo, token_start, token_end)
		if !flag_ok {
			return 0, false
		}
		modifiers^ |= flag
		index += 1
		cursor^ = index
		vk, ok := windows_parse_key_combination(combo, cursor, modifiers)
		if !ok {
			return 0, false
		}
		index = windows_combo_skip_ws(combo, cursor^)
		if index >= len(combo) || combo[index] != ')' {
			return 0, false
		}
		cursor^ = index + 1
		return vk, true
	}

	vk, vk_ok := windows_combo_keycode_from_token(combo, token_start, token_end)
	if !vk_ok {
		return 0, false
	}
	cursor^ = index
	return vk, true
}

windows_modifier_vk :: proc(modifier: int) -> win.WORD {
	switch modifier {
	case WINDOWS_COMBO_SHIFT:
		return win.WORD(win.VK_SHIFT)
	case WINDOWS_COMBO_CONTROL:
		return win.WORD(win.VK_CONTROL)
	case WINDOWS_COMBO_ALT:
		return win.WORD(win.VK_MENU)
	case WINDOWS_COMBO_SUPER:
		return win.WORD(win.VK_LWIN)
	}
	return 0
}

windows_emit_modifier :: proc(modifier: int, is_down: bool) -> bool {
	vk := windows_modifier_vk(modifier)
	if vk == 0 {
		return true
	}
	flags: win.DWORD
	if !is_down {
		flags = WINDOWS_KEYEVENTF_KEYUP
	}
	input := windows_keyboard_input_vk(vk, flags)
	inputs := [?]win.INPUT{input}
	return windows_send_inputs(inputs[:])
}

windows_emit_key_combination :: proc(modifiers: int, vk: win.WORD) -> bool {
	ok := true
	if (modifiers & WINDOWS_COMBO_SHIFT) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_SHIFT, true) && ok
	}
	if (modifiers & WINDOWS_COMBO_CONTROL) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_CONTROL, true) && ok
	}
	if (modifiers & WINDOWS_COMBO_ALT) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_ALT, true) && ok
	}
	if (modifiers & WINDOWS_COMBO_SUPER) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_SUPER, true) && ok
	}

	ok = windows_tap_vk(vk) && ok

	if (modifiers & WINDOWS_COMBO_SUPER) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_SUPER, false) && ok
	}
	if (modifiers & WINDOWS_COMBO_ALT) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_ALT, false) && ok
	}
	if (modifiers & WINDOWS_COMBO_CONTROL) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_CONTROL, false) && ok
	}
	if (modifiers & WINDOWS_COMBO_SHIFT) != 0 {
		ok = windows_emit_modifier(WINDOWS_COMBO_SHIFT, false) && ok
	}
	return ok
}

windows_send_key_combination :: proc(combo: string) -> bool {
	if len(combo) == 0 {
		return false
	}
	if !windows_output_init() {
		return false
	}
	cursor := 0
	modifiers := 0
	vk, ok := windows_parse_key_combination(combo, &cursor, &modifiers)
	if !ok {
		fmt.eprintln("stoin: unsupported key combo", combo)
		return false
	}
	cursor = windows_combo_skip_ws(combo, cursor)
	if cursor != len(combo) {
		fmt.eprintln("stoin: unsupported key combo", combo)
		return false
	}
	windows_report_translation_timing_before_output("key-combo")
	return windows_emit_key_combination(modifiers, vk)
}

windows_send_text_utf8 :: proc(text: string) -> bool {
	if len(text) == 0 {
		return true
	}
	if !windows_output_init() {
		return false
	}

	units := win.utf8_to_utf16(text, context.temp_allocator)
	if len(units) == 0 {
		return false
	}

	ok := true
	reported_timing := false
	for unit in units {
		inputs := [?]win.INPUT {
			windows_keyboard_input_unicode(unit, 0),
			windows_keyboard_input_unicode(unit, WINDOWS_KEYEVENTF_KEYUP),
		}
		if !reported_timing {
			windows_report_translation_timing_before_output("text")
			reported_timing = true
		}
		ok = windows_send_inputs(inputs[:]) && ok
	}
	return ok
}

windows_delete_text_utf8 :: proc(text: string) -> bool {
	if !windows_output_init() {
		return false
	}

	count := 0
	for _ in text {
		count += 1
	}
	if count > 0 {
		windows_report_translation_timing_before_output("delete")
	}
	for _ in 0..<count {
		if !windows_tap_vk(win.WORD(win.VK_BACK)) {
			return false
		}
	}
	return true
}

windows_runtime_send_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return windows_send_text_utf8(text)
}

windows_runtime_delete_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return windows_delete_text_utf8(text)
}

windows_runtime_send_key_combination :: proc(combo: string, userdata: rawptr) -> bool {
	_ = userdata
	return windows_send_key_combination(combo)
}
