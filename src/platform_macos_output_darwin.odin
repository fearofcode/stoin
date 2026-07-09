package stoin

import "core:c"
import "core:fmt"
import "core:time"
import CF "core:sys/darwin/CoreFoundation"
import "core:unicode/utf8"

foreign import CoreGraphics "system:CoreGraphics.framework"

CGEventRef :: distinct CF.TypeRef
CGEventSourceRef :: distinct CF.TypeRef
CFMachPortRef :: distinct CF.TypeRef
CGEventSourceStateID :: distinct i32
CGEventTapLocation :: distinct u32
CGEventTapPlacement :: distinct u32
CGEventTapOptions :: distinct u32
CGEventTapProxy :: distinct rawptr
CGEventType :: distinct u32
CGEventMask :: distinct u64
CGEventField :: distinct i32
CGEventFlags :: distinct u64
CGKeyCode :: distinct u16
UniCharCount :: distinct c.ulong
CGEventTapCallBack :: proc "c" (proxy: CGEventTapProxy, event_type: CGEventType, event: CGEventRef, user_info: rawptr) -> CGEventRef

MACOS_BACKSPACE_KEYCODE :: CGKeyCode(51)
MACOS_GENERATED_EVENT_USER_DATA :: i64(0x73746f696e)

KCG_EVENT_SOURCE_STATE_HID_SYSTEM :: CGEventSourceStateID(1)
KCG_SESSION_EVENT_TAP :: CGEventTapLocation(1)
KCG_HEAD_INSERT_EVENT_TAP :: CGEventTapPlacement(0)
KCG_EVENT_TAP_OPTION_DEFAULT :: CGEventTapOptions(0)
KCG_EVENT_TAP_OPTION_LISTEN_ONLY :: CGEventTapOptions(1)
KCG_EVENT_SOURCE_USER_DATA :: CGEventField(42)
KCG_KEYBOARD_EVENT_AUTOREPEAT :: CGEventField(8)
KCG_KEYBOARD_EVENT_KEYCODE :: CGEventField(9)

KCG_EVENT_KEY_DOWN :: CGEventType(10)
KCG_EVENT_KEY_UP :: CGEventType(11)
KCG_EVENT_FLAGS_CHANGED :: CGEventType(12)
KCG_EVENT_TAP_DISABLED_BY_TIMEOUT :: CGEventType(0xfffffffe)

KCG_EVENT_FLAG_MASK_SHIFT :: CGEventFlags(0x00020000)
KCG_EVENT_FLAG_MASK_CONTROL :: CGEventFlags(0x00040000)
KCG_EVENT_FLAG_MASK_ALTERNATE :: CGEventFlags(0x00080000)
KCG_EVENT_FLAG_MASK_COMMAND :: CGEventFlags(0x00100000)

foreign CoreGraphics {
	CGEventSourceCreate :: proc(state_id: CGEventSourceStateID) -> CGEventSourceRef ---
	CGEventCreateKeyboardEvent :: proc(source: CGEventSourceRef, virtual_key: CGKeyCode, key_down: bool) -> CGEventRef ---
	CGEventSetIntegerValueField :: proc(event: CGEventRef, field: CGEventField, value: i64) ---
	CGEventGetIntegerValueField :: proc(event: CGEventRef, field: CGEventField) -> i64 ---
	CGEventSetFlags :: proc(event: CGEventRef, flags: CGEventFlags) ---
	CGEventGetFlags :: proc(event: CGEventRef) -> CGEventFlags ---
	CGEventKeyboardSetUnicodeString :: proc(event: CGEventRef, string_length: UniCharCount, unicode_string: [^]u16) ---
	CGEventPost :: proc(tap: CGEventTapLocation, event: CGEventRef) ---
	CGEventTapCreate :: proc(tap: CGEventTapLocation, place: CGEventTapPlacement, options: CGEventTapOptions, events_of_interest: CGEventMask, callback: CGEventTapCallBack, user_info: rawptr) -> CFMachPortRef ---
	CGEventTapEnable :: proc(tap: CFMachPortRef, enable: bool) ---
}

macos_output_source: CGEventSourceRef
macos_translation_timing_enabled: bool
macos_translation_timing_active: bool
macos_translation_timing_start_ns: u64
macos_translation_timing_sequence: u64

macos_monotonic_ns :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec)
}

macos_translation_timing_set_enabled :: proc(enabled: bool) {
	macos_translation_timing_enabled = enabled
	if !enabled {
		macos_translation_timing_active = false
		macos_translation_timing_start_ns = 0
	}
}

macos_translation_timing_begin :: proc(start_ns: u64, userdata: rawptr) {
	_ = userdata
	if !macos_translation_timing_enabled || start_ns == 0 {
		return
	}
	macos_translation_timing_start_ns = start_ns
	macos_translation_timing_active = true
}

macos_translation_timing_cancel :: proc(userdata: rawptr) {
	_ = userdata
	macos_translation_timing_active = false
	macos_translation_timing_start_ns = 0
}

macos_report_translation_timing_before_output :: proc(operation: string) {
	if !macos_translation_timing_enabled || !macos_translation_timing_active {
		return
	}

	now_ns := macos_monotonic_ns()
	start_ns := macos_translation_timing_start_ns
	elapsed_ns: u64
	if now_ns >= start_ns {
		elapsed_ns = now_ns - start_ns
	}
	macos_translation_timing_active = false
	macos_translation_timing_start_ns = 0
	macos_translation_timing_sequence += 1

	label := operation
	if len(label) == 0 {
		label = "first"
	}
	fmt.eprintf(
		"stoin: translation latency #%d before %s CGEventPost: %.3f ms (%.1f us)\n",
		macos_translation_timing_sequence,
		label,
		f64(elapsed_ns) / 1_000_000.0,
		f64(elapsed_ns) / 1_000.0,
	)
}

macos_output_init :: proc() -> bool {
	if macos_output_source != nil {
		return true
	}
	macos_output_source = CGEventSourceCreate(KCG_EVENT_SOURCE_STATE_HID_SYSTEM)
	return macos_output_source != nil
}

macos_output_shutdown :: proc() {
	if macos_output_source != nil {
		CF.Release(CF.TypeRef(macos_output_source))
		macos_output_source = nil
	}
}

macos_mark_generated_event :: proc(event: CGEventRef) {
	if event != nil {
		CGEventSetIntegerValueField(event, KCG_EVENT_SOURCE_USER_DATA, MACOS_GENERATED_EVENT_USER_DATA)
	}
}

macos_event_was_generated_by_stoin :: proc(event: CGEventRef) -> bool {
	return event != nil && CGEventGetIntegerValueField(event, KCG_EVENT_SOURCE_USER_DATA) == MACOS_GENERATED_EVENT_USER_DATA
}

macos_post_keyboard_event_pair_with_flags_operation :: proc(keycode: CGKeyCode, flags: CGEventFlags, operation: string) -> bool {
	if !macos_output_init() {
		return false
	}

	key_down := CGEventCreateKeyboardEvent(macos_output_source, keycode, true)
	key_up := CGEventCreateKeyboardEvent(macos_output_source, keycode, false)
	ok := false
	if key_down != nil && key_up != nil {
		macos_mark_generated_event(key_down)
		macos_mark_generated_event(key_up)
		CGEventSetFlags(key_down, flags)
		CGEventSetFlags(key_up, flags)
		macos_report_translation_timing_before_output(operation)
		CGEventPost(KCG_SESSION_EVENT_TAP, key_down)
		CGEventPost(KCG_SESSION_EVENT_TAP, key_up)
		ok = true
	}

	if key_down != nil {
		CF.Release(CF.TypeRef(key_down))
	}
	if key_up != nil {
		CF.Release(CF.TypeRef(key_up))
	}
	return ok
}

macos_post_keyboard_event_pair_with_flags :: proc(keycode: CGKeyCode, flags: CGEventFlags) -> bool {
	return macos_post_keyboard_event_pair_with_flags_operation(keycode, flags, "key-combo")
}

macos_post_keyboard_event_pair :: proc(keycode: CGKeyCode) -> bool {
	return macos_post_keyboard_event_pair_with_flags_operation(keycode, 0, "key")
}

macos_append_rune_utf16 :: proc(out: ^[dynamic]u16, r: rune) {
	if r < 0x10000 {
		append(out, u16(r))
		return
	}
	if r > 0x10ffff {
		append(out, u16(0xfffd))
		return
	}
	value := u32(r) - 0x10000
	append(out, u16(0xd800 + (value >> 10)))
	append(out, u16(0xdc00 + (value & 0x3ff)))
}

macos_utf8_to_utf16 :: proc(text: string) -> (units: [dynamic]u16) {
	units = make([dynamic]u16)
	for r in text {
		macos_append_rune_utf16(&units, r)
	}
	return units
}

macos_send_text_utf8 :: proc(text: string) -> bool {
	if len(text) == 0 {
		return true
	}
	if !macos_output_init() {
		return false
	}

	units := macos_utf8_to_utf16(text)
	defer delete(units)

	key_down := CGEventCreateKeyboardEvent(macos_output_source, 0, true)
	key_up := CGEventCreateKeyboardEvent(macos_output_source, 0, false)
	ok := false
	if key_down != nil && key_up != nil {
		macos_mark_generated_event(key_down)
		macos_mark_generated_event(key_up)
		CGEventSetFlags(key_down, 0)
		CGEventSetFlags(key_up, 0)
		CGEventKeyboardSetUnicodeString(key_down, UniCharCount(len(units)), raw_data(units))
		CGEventKeyboardSetUnicodeString(key_up, UniCharCount(len(units)), raw_data(units))
		macos_report_translation_timing_before_output("text")
		CGEventPost(KCG_SESSION_EVENT_TAP, key_down)
		CGEventPost(KCG_SESSION_EVENT_TAP, key_up)
		ok = true
	}

	if key_down != nil {
		CF.Release(CF.TypeRef(key_down))
	}
	if key_up != nil {
		CF.Release(CF.TypeRef(key_up))
	}
	return ok
}

macos_delete_text_utf8 :: proc(text: string) -> bool {
	grapheme_count, _, _ := utf8.grapheme_count(text)
	for _ in 0..<grapheme_count {
		if !macos_post_keyboard_event_pair(MACOS_BACKSPACE_KEYCODE) {
			return false
		}
	}
	return true
}

macos_combo_token_equals :: proc(combo: string, start: int, end: int, expected: string) -> bool {
	return keymap_ascii_equal_ignore_case(combo[start:end], expected)
}

macos_combo_skip_ws :: proc(combo: string, cursor: int) -> int {
	index := cursor
	for index < len(combo) && keymap_is_space(combo[index]) {
		index += 1
	}
	return index
}

macos_combo_is_token_byte :: proc(c: byte) -> bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '_'
}

macos_combo_modifier_flag :: proc(combo: string, start: int, end: int) -> (CGEventFlags, bool) {
	if macos_combo_token_equals(combo, start, end, "shift") ||
	   macos_combo_token_equals(combo, start, end, "shift_l") ||
	   macos_combo_token_equals(combo, start, end, "shift_r") {
		return KCG_EVENT_FLAG_MASK_SHIFT, true
	}
	if macos_combo_token_equals(combo, start, end, "control") ||
	   macos_combo_token_equals(combo, start, end, "control_l") ||
	   macos_combo_token_equals(combo, start, end, "control_r") ||
	   macos_combo_token_equals(combo, start, end, "ctrl") {
		return KCG_EVENT_FLAG_MASK_CONTROL, true
	}
	if macos_combo_token_equals(combo, start, end, "alt") ||
	   macos_combo_token_equals(combo, start, end, "alt_l") ||
	   macos_combo_token_equals(combo, start, end, "alt_r") ||
	   macos_combo_token_equals(combo, start, end, "option") ||
	   macos_combo_token_equals(combo, start, end, "option_l") ||
	   macos_combo_token_equals(combo, start, end, "option_r") {
		return KCG_EVENT_FLAG_MASK_ALTERNATE, true
	}
	if macos_combo_token_equals(combo, start, end, "command") ||
	   macos_combo_token_equals(combo, start, end, "super") ||
	   macos_combo_token_equals(combo, start, end, "super_l") ||
	   macos_combo_token_equals(combo, start, end, "super_r") ||
	   macos_combo_token_equals(combo, start, end, "windows") {
		return KCG_EVENT_FLAG_MASK_COMMAND, true
	}
	return 0, false
}

macos_combo_parse_function_key :: proc(combo: string, start: int, end: int) -> (CGKeyCode, bool) {
	if end - start < 2 || keymap_ascii_to_lower(combo[start]) != 'f' {
		return 0, false
	}
	value := 0
	for i := start + 1; i < end; i += 1 {
		if combo[i] < '0' || combo[i] > '9' {
			return 0, false
		}
		value = value * 10 + int(combo[i] - '0')
	}
	switch value {
	case 1: return 122, true
	case 2: return 120, true
	case 3: return 99, true
	case 4: return 118, true
	case 5: return 96, true
	case 6: return 97, true
	case 7: return 98, true
	case 8: return 100, true
	case 9: return 101, true
	case 10: return 109, true
	case 11: return 103, true
	case 12: return 111, true
	}
	return 0, false
}

macos_combo_keycode_from_token :: proc(combo: string, start: int, end: int) -> (CGKeyCode, bool) {
	if end - start == 1 {
		key_name := combo[start:end]
		keycode, ok := keycode_from_name(key_name)
		return CGKeyCode(keycode), ok
	}
	if macos_combo_token_equals(combo, start, end, "home") {
		return 115, true
	}
	if macos_combo_token_equals(combo, start, end, "end") {
		return 119, true
	}
	if macos_combo_token_equals(combo, start, end, "page_up") || macos_combo_token_equals(combo, start, end, "pageup") {
		return 116, true
	}
	if macos_combo_token_equals(combo, start, end, "page_down") || macos_combo_token_equals(combo, start, end, "pagedown") {
		return 121, true
	}
	if macos_combo_token_equals(combo, start, end, "left") {
		return 123, true
	}
	if macos_combo_token_equals(combo, start, end, "right") {
		return 124, true
	}
	if macos_combo_token_equals(combo, start, end, "down") {
		return 125, true
	}
	if macos_combo_token_equals(combo, start, end, "up") {
		return 126, true
	}
	if macos_combo_token_equals(combo, start, end, "tab") {
		return 48, true
	}
	if macos_combo_token_equals(combo, start, end, "return") || macos_combo_token_equals(combo, start, end, "enter") {
		return 36, true
	}
	if macos_combo_token_equals(combo, start, end, "backspace") {
		return 51, true
	}
	if macos_combo_token_equals(combo, start, end, "delete") {
		return 117, true
	}
	if macos_combo_token_equals(combo, start, end, "escape") || macos_combo_token_equals(combo, start, end, "esc") {
		return 53, true
	}
	return macos_combo_parse_function_key(combo, start, end)
}

macos_parse_combo_expression :: proc(combo: string, cursor: ^int, flags: ^CGEventFlags, keycode: ^CGKeyCode) -> bool {
	p := macos_combo_skip_ws(combo, cursor^)
	token_start := p
	for p < len(combo) && macos_combo_is_token_byte(combo[p]) {
		p += 1
	}
	if p == token_start {
		return false
	}
	token_end := p
	p = macos_combo_skip_ws(combo, p)
	if p < len(combo) && combo[p] == '(' {
		flag, flag_ok := macos_combo_modifier_flag(combo, token_start, token_end)
		if !flag_ok {
			return false
		}
		flags^ = flags^ | flag
		p += 1
		if !macos_parse_combo_expression(combo, &p, flags, keycode) {
			return false
		}
		p = macos_combo_skip_ws(combo, p)
		if p >= len(combo) || combo[p] != ')' {
			return false
		}
		cursor^ = p + 1
		return true
	}

	next_keycode, key_ok := macos_combo_keycode_from_token(combo, token_start, token_end)
	if !key_ok {
		return false
	}
	keycode^ = next_keycode
	cursor^ = p
	return true
}

macos_parse_key_combination :: proc(combo: string) -> (keycode: CGKeyCode, flags: CGEventFlags, ok: bool) {
	cursor := 0
	if !macos_parse_combo_expression(combo, &cursor, &flags, &keycode) {
		return 0, 0, false
	}
	cursor = macos_combo_skip_ws(combo, cursor)
	return keycode, flags, cursor == len(combo)
}

macos_send_key_combination :: proc(combo: string) -> bool {
	keycode, flags, ok := macos_parse_key_combination(combo)
	if !ok {
		return false
	}
	return macos_post_keyboard_event_pair_with_flags(keycode, flags)
}

macos_runtime_send_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return macos_send_text_utf8(text)
}

macos_runtime_delete_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return macos_delete_text_utf8(text)
}

macos_runtime_send_key_combination :: proc(combo: string, userdata: rawptr) -> bool {
	_ = userdata
	return macos_send_key_combination(combo)
}
