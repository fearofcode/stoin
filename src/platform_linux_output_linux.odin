#+build linux
package stoin

import "core:fmt"
import "core:mem"
import "core:strings"
import "core:sys/linux"

LINUX_UINPUT_NAME :: "stoin virtual keyboard"
LINUX_UINPUT_PATH :: "/dev/uinput"

LINUX_EV_SYN :: 0
LINUX_EV_KEY :: 1
LINUX_SYN_REPORT :: 0

LINUX_KEY_ESC :: 1
LINUX_KEY_1 :: 2
LINUX_KEY_2 :: 3
LINUX_KEY_3 :: 4
LINUX_KEY_4 :: 5
LINUX_KEY_5 :: 6
LINUX_KEY_6 :: 7
LINUX_KEY_7 :: 8
LINUX_KEY_8 :: 9
LINUX_KEY_9 :: 10
LINUX_KEY_0 :: 11
LINUX_KEY_MINUS :: 12
LINUX_KEY_EQUAL :: 13
LINUX_KEY_BACKSPACE :: 14
LINUX_KEY_TAB :: 15
LINUX_KEY_Q :: 16
LINUX_KEY_W :: 17
LINUX_KEY_E :: 18
LINUX_KEY_R :: 19
LINUX_KEY_T :: 20
LINUX_KEY_Y :: 21
LINUX_KEY_U :: 22
LINUX_KEY_I :: 23
LINUX_KEY_O :: 24
LINUX_KEY_P :: 25
LINUX_KEY_LEFTBRACE :: 26
LINUX_KEY_RIGHTBRACE :: 27
LINUX_KEY_ENTER :: 28
LINUX_KEY_LEFTCTRL :: 29
LINUX_KEY_A :: 30
LINUX_KEY_S :: 31
LINUX_KEY_D :: 32
LINUX_KEY_F :: 33
LINUX_KEY_G :: 34
LINUX_KEY_H :: 35
LINUX_KEY_J :: 36
LINUX_KEY_K :: 37
LINUX_KEY_L :: 38
LINUX_KEY_SEMICOLON :: 39
LINUX_KEY_APOSTROPHE :: 40
LINUX_KEY_GRAVE :: 41
LINUX_KEY_LEFTSHIFT :: 42
LINUX_KEY_BACKSLASH :: 43
LINUX_KEY_Z :: 44
LINUX_KEY_X :: 45
LINUX_KEY_C :: 46
LINUX_KEY_V :: 47
LINUX_KEY_B :: 48
LINUX_KEY_N :: 49
LINUX_KEY_M :: 50
LINUX_KEY_COMMA :: 51
LINUX_KEY_DOT :: 52
LINUX_KEY_SLASH :: 53
LINUX_KEY_RIGHTSHIFT :: 54
LINUX_KEY_LEFTALT :: 56
LINUX_KEY_SPACE :: 57
LINUX_KEY_F1 :: 59
LINUX_KEY_F2 :: 60
LINUX_KEY_F3 :: 61
LINUX_KEY_F4 :: 62
LINUX_KEY_F5 :: 63
LINUX_KEY_F6 :: 64
LINUX_KEY_F7 :: 65
LINUX_KEY_F8 :: 66
LINUX_KEY_F9 :: 67
LINUX_KEY_F10 :: 68
LINUX_KEY_F11 :: 87
LINUX_KEY_F12 :: 88
LINUX_KEY_RIGHTCTRL :: 97
LINUX_KEY_RIGHTALT :: 100
LINUX_KEY_HOME :: 102
LINUX_KEY_UP :: 103
LINUX_KEY_PAGEUP :: 104
LINUX_KEY_LEFT :: 105
LINUX_KEY_RIGHT :: 106
LINUX_KEY_END :: 107
LINUX_KEY_DOWN :: 108
LINUX_KEY_PAGEDOWN :: 109
LINUX_KEY_DELETE :: 111
LINUX_KEY_LEFTMETA :: 125
LINUX_KEY_RIGHTMETA :: 126
LINUX_KEY_F13 :: 183
LINUX_KEY_F14 :: 184
LINUX_KEY_F15 :: 185
LINUX_KEY_F16 :: 186
LINUX_KEY_F17 :: 187
LINUX_KEY_F18 :: 188
LINUX_KEY_F19 :: 189
LINUX_KEY_F20 :: 190
LINUX_KEY_MAX :: 0x2ff

LINUX_BUS_USB :: u16(0x03)
LINUX_UINPUT_MAX_NAME_SIZE :: 80
LINUX_UINPUT_IOCTL_BASE :: byte('U')
// Linux UAPI ioctl definitions use a 32-bit C int. Odin's int is
// pointer-sized, so using size_of(int) changes the request number on 64-bit
// systems and makes the kernel reject the ioctl with ENOTTY.
LINUX_C_INT_SIZE :: u32(size_of(i32))

LINUX_COMBO_SHIFT :: 1 << 0
LINUX_COMBO_CONTROL :: 1 << 1
LINUX_COMBO_ALT :: 1 << 2
LINUX_COMBO_SUPER :: 1 << 3

Linux_Input_Event :: struct {
	tv_sec:  i64,
	tv_usec: i64,
	type:    u16,
	code:    u16,
	value:   i32,
}

Linux_Input_Id :: struct {
	bustype: u16,
	vendor:  u16,
	product: u16,
	version: u16,
}

Linux_Uinput_Setup :: struct {
	id:             Linux_Input_Id,
	name:           [LINUX_UINPUT_MAX_NAME_SIZE]byte,
	ff_effects_max: u32,
}

Linux_Ascii_Key :: struct {
	keycode: int,
	shift:   bool,
}

linux_output_fd: linux.Fd = linux.Fd(-1)
linux_translation_timing_enabled: bool
linux_translation_timing_active: bool
linux_translation_timing_start_ns: u64
linux_translation_timing_sequence: u64

linux_ioc :: proc(dir, request_type, nr, size: u32) -> u32 {
	return (dir << 30) | (request_type << 8) | nr | (size << 16)
}

linux_io :: proc(request_type, nr: u32) -> u32 {
	return linux_ioc(0, request_type, nr, 0)
}

linux_iow :: proc(request_type, nr, size: u32) -> u32 {
	return linux_ioc(1, request_type, nr, size)
}

LINUX_UI_DEV_CREATE :: (0 << 30) | (u32(LINUX_UINPUT_IOCTL_BASE) << 8) | 1 | (0 << 16)
LINUX_UI_DEV_DESTROY :: (0 << 30) | (u32(LINUX_UINPUT_IOCTL_BASE) << 8) | 2 | (0 << 16)
LINUX_UI_DEV_SETUP :: (1 << 30) | (u32(LINUX_UINPUT_IOCTL_BASE) << 8) | 3 | (u32(size_of(Linux_Uinput_Setup)) << 16)
LINUX_UI_SET_EVBIT :: (1 << 30) | (u32(LINUX_UINPUT_IOCTL_BASE) << 8) | 100 | (LINUX_C_INT_SIZE << 16)
LINUX_UI_SET_KEYBIT :: (1 << 30) | (u32(LINUX_UINPUT_IOCTL_BASE) << 8) | 101 | (LINUX_C_INT_SIZE << 16)

#assert(LINUX_UI_DEV_CREATE == 0x00005501)
#assert(LINUX_UI_DEV_DESTROY == 0x00005502)
#assert(size_of(Linux_Uinput_Setup) == 92)
#assert(LINUX_UI_DEV_SETUP == 0x405c5503)
#assert(LINUX_UI_SET_EVBIT == 0x40045564)
#assert(LINUX_UI_SET_KEYBIT == 0x40045565)

linux_ioctl_errno :: proc(fd: linux.Fd, request: u32, arg: uintptr) -> linux.Errno {
	result := int(linux.ioctl(fd, request, arg))
	if result < 0 {
		return linux.Errno(-result)
	}
	return .NONE
}

linux_ioctl_ok :: proc(fd: linux.Fd, request: u32, arg: uintptr) -> bool {
	return linux_ioctl_errno(fd, request, arg) == .NONE
}

linux_uinput_ioctl_ok :: proc(fd: linux.Fd, request: u32, arg: uintptr, operation: string) -> bool {
	err := linux_ioctl_errno(fd, request, arg)
	if err == .NONE {
		return true
	}
	fmt.eprintln("stoin: Linux uinput", operation, "failed:", err)
	return false
}

linux_write_all :: proc(fd: linux.Fd, data: []byte) -> bool {
	written := 0
	for written < len(data) {
		n, err := linux.write(fd, data[written:])
		if err == .EINTR || err == .EAGAIN {
			continue
		}
		if err != .NONE || n <= 0 {
			return false
		}
		written += n
	}
	return true
}

linux_write_input_event :: proc(event_type, code, value: int) -> bool {
	if int(linux_output_fd) < 0 {
		return false
	}
	event := Linux_Input_Event {
		type = u16(event_type),
		code = u16(code),
		value = i32(value),
	}
	return linux_write_all(linux_output_fd, mem.ptr_to_bytes(&event))
}

linux_emit_syn_report :: proc() -> bool {
	return linux_write_input_event(LINUX_EV_SYN, LINUX_SYN_REPORT, 0)
}

linux_uinput_emit_key_event :: proc(keycode: int, value: int) -> bool {
	return linux_write_input_event(LINUX_EV_KEY, keycode, value) && linux_emit_syn_report()
}

linux_uinput_tap_key :: proc(keycode: int) -> bool {
	return linux_uinput_emit_key_event(keycode, 1) && linux_uinput_emit_key_event(keycode, 0)
}

linux_copy_name :: proc(dst: ^[LINUX_UINPUT_MAX_NAME_SIZE]byte, text: string) {
	limit := min(len(text), LINUX_UINPUT_MAX_NAME_SIZE - 1)
	for i in 0..<limit {
		dst[i] = text[i]
	}
}

linux_output_init :: proc() -> bool {
	if int(linux_output_fd) >= 0 {
		return true
	}

	cpath, cpath_err := strings.clone_to_cstring(LINUX_UINPUT_PATH)
	if cpath_err != nil {
		return false
	}
	defer delete(cpath)

	fd, open_err := linux.open(cpath, {.WRONLY, .NONBLOCK, .CLOEXEC})
	if open_err != .NONE {
		fmt.eprintln("stoin: failed to open /dev/uinput for Linux keyboard output:", open_err)
		fmt.eprintln("stoin: check uinput permissions or run with access to /dev/uinput")
		return false
	}

	if !linux_uinput_ioctl_ok(fd, LINUX_UI_SET_EVBIT, uintptr(LINUX_EV_KEY), "UI_SET_EVBIT(EV_KEY)") ||
	   !linux_uinput_ioctl_ok(fd, LINUX_UI_SET_EVBIT, uintptr(LINUX_EV_SYN), "UI_SET_EVBIT(EV_SYN)") {
		linux.close(fd)
		return false
	}
	for key in 1..<LINUX_KEY_MAX {
		_ = linux_ioctl_ok(fd, LINUX_UI_SET_KEYBIT, uintptr(key))
	}

	setup: Linux_Uinput_Setup
	setup.id.bustype = LINUX_BUS_USB
	setup.id.vendor = 0x7374
	setup.id.product = 0x6f69
	setup.id.version = 1
	linux_copy_name(&setup.name, LINUX_UINPUT_NAME)

	if !linux_uinput_ioctl_ok(fd, LINUX_UI_DEV_SETUP, uintptr(rawptr(&setup)), "UI_DEV_SETUP") ||
	   !linux_uinput_ioctl_ok(fd, LINUX_UI_DEV_CREATE, 0, "UI_DEV_CREATE") {
		linux.close(fd)
		return false
	}

	linux_output_fd = fd
	linux_sleep_ms(100)
	return true
}

linux_output_shutdown :: proc() {
	if int(linux_output_fd) < 0 {
		return
	}
	_ = linux_ioctl_ok(linux_output_fd, LINUX_UI_DEV_DESTROY, 0)
	linux.close(linux_output_fd)
	linux_output_fd = linux.Fd(-1)
}

linux_monotonic_ns :: proc() -> u64 {
	ts, err := linux.clock_gettime(.MONOTONIC)
	if err != .NONE {
		return 0
	}
	return u64(ts.time_sec) * 1_000_000_000 + u64(ts.time_nsec)
}

linux_sleep_ms :: proc(milliseconds: uint) {
	requested := linux.Time_Spec {
		time_sec = milliseconds / 1000,
		time_nsec = (milliseconds % 1000) * 1_000_000,
	}
	for {
		remaining := requested
		err := linux.nanosleep(&requested, &remaining)
		if err != .EINTR {
			break
		}
		requested = remaining
	}
}

linux_translation_timing_set_enabled :: proc(enabled: bool) {
	linux_translation_timing_enabled = enabled
	if !enabled {
		linux_translation_timing_active = false
		linux_translation_timing_start_ns = 0
	}
}

linux_translation_timing_begin :: proc(start_ns: u64, userdata: rawptr) {
	_ = userdata
	if !linux_translation_timing_enabled || start_ns == 0 {
		return
	}
	linux_translation_timing_start_ns = start_ns
	linux_translation_timing_active = true
}

linux_translation_timing_cancel :: proc(userdata: rawptr) {
	_ = userdata
	linux_translation_timing_active = false
	linux_translation_timing_start_ns = 0
}

linux_report_translation_timing_before_output :: proc(operation: string) {
	if !linux_translation_timing_enabled || !linux_translation_timing_active {
		return
	}
	now_ns := linux_monotonic_ns()
	start_ns := linux_translation_timing_start_ns
	elapsed_ns: u64
	if now_ns >= start_ns {
		elapsed_ns = now_ns - start_ns
	}
	linux_translation_timing_active = false
	linux_translation_timing_start_ns = 0
	linux_translation_timing_sequence += 1

	label := operation
	if len(label) == 0 {
		label = "first"
	}
	fmt.eprintf(
		"stoin: translation latency #%d before %s uinput event: %.3f ms (%.1f us)\n",
		linux_translation_timing_sequence,
		label,
		f64(elapsed_ns) / 1_000_000.0,
		f64(elapsed_ns) / 1_000.0,
	)
}

linux_letter_keycode :: proc(letter: rune) -> (int, bool) {
	switch letter {
	case 'a': return LINUX_KEY_A, true
	case 'b': return LINUX_KEY_B, true
	case 'c': return LINUX_KEY_C, true
	case 'd': return LINUX_KEY_D, true
	case 'e': return LINUX_KEY_E, true
	case 'f': return LINUX_KEY_F, true
	case 'g': return LINUX_KEY_G, true
	case 'h': return LINUX_KEY_H, true
	case 'i': return LINUX_KEY_I, true
	case 'j': return LINUX_KEY_J, true
	case 'k': return LINUX_KEY_K, true
	case 'l': return LINUX_KEY_L, true
	case 'm': return LINUX_KEY_M, true
	case 'n': return LINUX_KEY_N, true
	case 'o': return LINUX_KEY_O, true
	case 'p': return LINUX_KEY_P, true
	case 'q': return LINUX_KEY_Q, true
	case 'r': return LINUX_KEY_R, true
	case 's': return LINUX_KEY_S, true
	case 't': return LINUX_KEY_T, true
	case 'u': return LINUX_KEY_U, true
	case 'v': return LINUX_KEY_V, true
	case 'w': return LINUX_KEY_W, true
	case 'x': return LINUX_KEY_X, true
	case 'y': return LINUX_KEY_Y, true
	case 'z': return LINUX_KEY_Z, true
	}
	return 0, false
}

linux_ascii_key_from_rune :: proc(r: rune) -> (Linux_Ascii_Key, bool) {
	if r >= 'a' && r <= 'z' {
		keycode, ok := linux_letter_keycode(r)
		return Linux_Ascii_Key{keycode = keycode}, ok
	}
	if r >= 'A' && r <= 'Z' {
		keycode, ok := linux_letter_keycode(r + ('a' - 'A'))
		return Linux_Ascii_Key{keycode = keycode, shift = true}, ok
	}

	switch r {
	case '1': return Linux_Ascii_Key{keycode = LINUX_KEY_1}, true
	case '2': return Linux_Ascii_Key{keycode = LINUX_KEY_2}, true
	case '3': return Linux_Ascii_Key{keycode = LINUX_KEY_3}, true
	case '4': return Linux_Ascii_Key{keycode = LINUX_KEY_4}, true
	case '5': return Linux_Ascii_Key{keycode = LINUX_KEY_5}, true
	case '6': return Linux_Ascii_Key{keycode = LINUX_KEY_6}, true
	case '7': return Linux_Ascii_Key{keycode = LINUX_KEY_7}, true
	case '8': return Linux_Ascii_Key{keycode = LINUX_KEY_8}, true
	case '9': return Linux_Ascii_Key{keycode = LINUX_KEY_9}, true
	case '0': return Linux_Ascii_Key{keycode = LINUX_KEY_0}, true
	case '!': return Linux_Ascii_Key{keycode = LINUX_KEY_1, shift = true}, true
	case '@': return Linux_Ascii_Key{keycode = LINUX_KEY_2, shift = true}, true
	case '#': return Linux_Ascii_Key{keycode = LINUX_KEY_3, shift = true}, true
	case '$': return Linux_Ascii_Key{keycode = LINUX_KEY_4, shift = true}, true
	case '%': return Linux_Ascii_Key{keycode = LINUX_KEY_5, shift = true}, true
	case '^': return Linux_Ascii_Key{keycode = LINUX_KEY_6, shift = true}, true
	case '&': return Linux_Ascii_Key{keycode = LINUX_KEY_7, shift = true}, true
	case '*': return Linux_Ascii_Key{keycode = LINUX_KEY_8, shift = true}, true
	case '(': return Linux_Ascii_Key{keycode = LINUX_KEY_9, shift = true}, true
	case ')': return Linux_Ascii_Key{keycode = LINUX_KEY_0, shift = true}, true
	case ' ': return Linux_Ascii_Key{keycode = LINUX_KEY_SPACE}, true
	case '\n': return Linux_Ascii_Key{keycode = LINUX_KEY_ENTER}, true
	case '\t': return Linux_Ascii_Key{keycode = LINUX_KEY_TAB}, true
	case '-': return Linux_Ascii_Key{keycode = LINUX_KEY_MINUS}, true
	case '_': return Linux_Ascii_Key{keycode = LINUX_KEY_MINUS, shift = true}, true
	case '=': return Linux_Ascii_Key{keycode = LINUX_KEY_EQUAL}, true
	case '+': return Linux_Ascii_Key{keycode = LINUX_KEY_EQUAL, shift = true}, true
	case '[': return Linux_Ascii_Key{keycode = LINUX_KEY_LEFTBRACE}, true
	case '{': return Linux_Ascii_Key{keycode = LINUX_KEY_LEFTBRACE, shift = true}, true
	case ']': return Linux_Ascii_Key{keycode = LINUX_KEY_RIGHTBRACE}, true
	case '}': return Linux_Ascii_Key{keycode = LINUX_KEY_RIGHTBRACE, shift = true}, true
	case '\\': return Linux_Ascii_Key{keycode = LINUX_KEY_BACKSLASH}, true
	case '|': return Linux_Ascii_Key{keycode = LINUX_KEY_BACKSLASH, shift = true}, true
	case ';': return Linux_Ascii_Key{keycode = LINUX_KEY_SEMICOLON}, true
	case ':': return Linux_Ascii_Key{keycode = LINUX_KEY_SEMICOLON, shift = true}, true
	case '\'': return Linux_Ascii_Key{keycode = LINUX_KEY_APOSTROPHE}, true
	case '"': return Linux_Ascii_Key{keycode = LINUX_KEY_APOSTROPHE, shift = true}, true
	case ',': return Linux_Ascii_Key{keycode = LINUX_KEY_COMMA}, true
	case '<': return Linux_Ascii_Key{keycode = LINUX_KEY_COMMA, shift = true}, true
	case '.': return Linux_Ascii_Key{keycode = LINUX_KEY_DOT}, true
	case '>': return Linux_Ascii_Key{keycode = LINUX_KEY_DOT, shift = true}, true
	case '/': return Linux_Ascii_Key{keycode = LINUX_KEY_SLASH}, true
	case '?': return Linux_Ascii_Key{keycode = LINUX_KEY_SLASH, shift = true}, true
	case '`': return Linux_Ascii_Key{keycode = LINUX_KEY_GRAVE}, true
	case '~': return Linux_Ascii_Key{keycode = LINUX_KEY_GRAVE, shift = true}, true
	}
	return {}, false
}

linux_send_ascii_key :: proc(key: Linux_Ascii_Key) -> bool {
	ok := true
	if key.shift {
		ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTSHIFT, 1) && ok
	}
	ok = linux_uinput_tap_key(key.keycode) && ok
	if key.shift {
		ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTSHIFT, 0) && ok
	}
	return ok
}

linux_hex_digit_keycode :: proc(digit: byte) -> (int, bool) {
	switch digit {
	case '0': return LINUX_KEY_0, true
	case '1': return LINUX_KEY_1, true
	case '2': return LINUX_KEY_2, true
	case '3': return LINUX_KEY_3, true
	case '4': return LINUX_KEY_4, true
	case '5': return LINUX_KEY_5, true
	case '6': return LINUX_KEY_6, true
	case '7': return LINUX_KEY_7, true
	case '8': return LINUX_KEY_8, true
	case '9': return LINUX_KEY_9, true
	case 'a': return LINUX_KEY_A, true
	case 'b': return LINUX_KEY_B, true
	case 'c': return LINUX_KEY_C, true
	case 'd': return LINUX_KEY_D, true
	case 'e': return LINUX_KEY_E, true
	case 'f': return LINUX_KEY_F, true
	}
	return 0, false
}

linux_send_unicode_codepoint :: proc(codepoint: u32) -> bool {
	hex := fmt.aprintf("%x", codepoint)
	defer delete(hex)

	ok := true
	ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTCTRL, 1) && ok
	ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTSHIFT, 1) && ok
	ok = linux_uinput_tap_key(LINUX_KEY_U) && ok
	ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTSHIFT, 0) && ok
	ok = linux_uinput_emit_key_event(LINUX_KEY_LEFTCTRL, 0) && ok
	if !ok {
		return false
	}

	for c in hex {
		keycode, key_ok := linux_hex_digit_keycode(byte(c))
		if !key_ok || !linux_uinput_tap_key(keycode) {
			return false
		}
	}
	return linux_uinput_tap_key(LINUX_KEY_SPACE)
}

linux_send_text_utf8 :: proc(text: string) -> bool {
	if len(text) == 0 {
		return true
	}
	if !linux_output_init() {
		return false
	}

	reported_timing := false
	for r in text {
		if !reported_timing {
			linux_report_translation_timing_before_output("text")
			reported_timing = true
		}
		if key, key_ok := linux_ascii_key_from_rune(r); key_ok {
			if !linux_send_ascii_key(key) {
				return false
			}
		} else if !linux_send_unicode_codepoint(u32(r)) {
			return false
		}
	}
	return true
}

linux_delete_text_utf8 :: proc(text: string) -> bool {
	if !linux_output_init() {
		return false
	}

	count := 0
	for _ in text {
		count += 1
	}
	if count > 0 {
		linux_report_translation_timing_before_output("delete")
	}
	for _ in 0..<count {
		if !linux_uinput_tap_key(LINUX_KEY_BACKSPACE) {
			return false
		}
	}
	return true
}

linux_logical_keycode_to_evdev :: proc(logical: u16) -> (int, bool) {
	switch logical {
	case 0: return LINUX_KEY_A, true
	case 1: return LINUX_KEY_S, true
	case 2: return LINUX_KEY_D, true
	case 3: return LINUX_KEY_F, true
	case 4: return LINUX_KEY_H, true
	case 5: return LINUX_KEY_G, true
	case 6: return LINUX_KEY_Z, true
	case 7: return LINUX_KEY_X, true
	case 8: return LINUX_KEY_C, true
	case 9: return LINUX_KEY_V, true
	case 11: return LINUX_KEY_B, true
	case 12: return LINUX_KEY_Q, true
	case 13: return LINUX_KEY_W, true
	case 14: return LINUX_KEY_E, true
	case 15: return LINUX_KEY_R, true
	case 16: return LINUX_KEY_Y, true
	case 17: return LINUX_KEY_T, true
	case 18: return LINUX_KEY_1, true
	case 19: return LINUX_KEY_2, true
	case 20: return LINUX_KEY_3, true
	case 21: return LINUX_KEY_4, true
	case 22: return LINUX_KEY_6, true
	case 23: return LINUX_KEY_5, true
	case 25: return LINUX_KEY_9, true
	case 26: return LINUX_KEY_7, true
	case 28: return LINUX_KEY_8, true
	case 29: return LINUX_KEY_0, true
	case 30: return LINUX_KEY_RIGHTBRACE, true
	case 31: return LINUX_KEY_O, true
	case 32: return LINUX_KEY_U, true
	case 33: return LINUX_KEY_LEFTBRACE, true
	case 34: return LINUX_KEY_I, true
	case 35: return LINUX_KEY_P, true
	case 36: return LINUX_KEY_ENTER, true
	case 37: return LINUX_KEY_L, true
	case 38: return LINUX_KEY_J, true
	case 39: return LINUX_KEY_APOSTROPHE, true
	case 40: return LINUX_KEY_K, true
	case 41: return LINUX_KEY_SEMICOLON, true
	case 42: return LINUX_KEY_BACKSLASH, true
	case 43: return LINUX_KEY_COMMA, true
	case 44: return LINUX_KEY_SLASH, true
	case 45: return LINUX_KEY_N, true
	case 46: return LINUX_KEY_M, true
	case 47: return LINUX_KEY_DOT, true
	case 48: return LINUX_KEY_TAB, true
	case 49: return LINUX_KEY_SPACE, true
	case 50: return LINUX_KEY_GRAVE, true
	case 51: return LINUX_KEY_BACKSPACE, true
	case 53: return LINUX_KEY_ESC, true
	case 54: return LINUX_KEY_RIGHTMETA, true
	case 55: return LINUX_KEY_LEFTMETA, true
	case 56: return LINUX_KEY_LEFTSHIFT, true
	case 58: return LINUX_KEY_LEFTALT, true
	case 59: return LINUX_KEY_LEFTCTRL, true
	case 60: return LINUX_KEY_RIGHTSHIFT, true
	case 61: return LINUX_KEY_RIGHTALT, true
	case 62: return LINUX_KEY_RIGHTCTRL, true
	case 64: return LINUX_KEY_F17, true
	case 79: return LINUX_KEY_F18, true
	case 80: return LINUX_KEY_F19, true
	case 90: return LINUX_KEY_F20, true
	case 105: return LINUX_KEY_F13, true
	case 106: return LINUX_KEY_F16, true
	case 107: return LINUX_KEY_F14, true
	case 113: return LINUX_KEY_F15, true
	}
	return 0, false
}

linux_combo_token_equals :: proc(combo: string, start, end: int, expected: string) -> bool {
	return keymap_ascii_equal_ignore_case(combo[start:end], expected)
}

linux_combo_skip_ws :: proc(combo: string, cursor: int) -> int {
	index := cursor
	for index < len(combo) && keymap_is_space(combo[index]) {
		index += 1
	}
	return index
}

linux_combo_is_token_byte :: proc(c: byte) -> bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '_'
}

linux_combo_modifier_flag :: proc(combo: string, start, end: int) -> (int, bool) {
	if linux_combo_token_equals(combo, start, end, "shift") ||
	   linux_combo_token_equals(combo, start, end, "shift_l") ||
	   linux_combo_token_equals(combo, start, end, "shift_r") {
		return LINUX_COMBO_SHIFT, true
	}
	if linux_combo_token_equals(combo, start, end, "control") ||
	   linux_combo_token_equals(combo, start, end, "control_l") ||
	   linux_combo_token_equals(combo, start, end, "control_r") ||
	   linux_combo_token_equals(combo, start, end, "ctrl") {
		return LINUX_COMBO_CONTROL, true
	}
	if linux_combo_token_equals(combo, start, end, "alt") ||
	   linux_combo_token_equals(combo, start, end, "alt_l") ||
	   linux_combo_token_equals(combo, start, end, "alt_r") ||
	   linux_combo_token_equals(combo, start, end, "option") ||
	   linux_combo_token_equals(combo, start, end, "option_l") ||
	   linux_combo_token_equals(combo, start, end, "option_r") {
		return LINUX_COMBO_ALT, true
	}
	if linux_combo_token_equals(combo, start, end, "command") ||
	   linux_combo_token_equals(combo, start, end, "super") ||
	   linux_combo_token_equals(combo, start, end, "super_l") ||
	   linux_combo_token_equals(combo, start, end, "super_r") ||
	   linux_combo_token_equals(combo, start, end, "windows") {
		return LINUX_COMBO_SUPER, true
	}
	return 0, false
}

linux_combo_parse_function_key :: proc(combo: string, start, end: int) -> (int, bool) {
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
	case 1: return LINUX_KEY_F1, true
	case 2: return LINUX_KEY_F2, true
	case 3: return LINUX_KEY_F3, true
	case 4: return LINUX_KEY_F4, true
	case 5: return LINUX_KEY_F5, true
	case 6: return LINUX_KEY_F6, true
	case 7: return LINUX_KEY_F7, true
	case 8: return LINUX_KEY_F8, true
	case 9: return LINUX_KEY_F9, true
	case 10: return LINUX_KEY_F10, true
	case 11: return LINUX_KEY_F11, true
	case 12: return LINUX_KEY_F12, true
	}
	return 0, false
}

linux_combo_keycode_from_token :: proc(combo: string, start, end: int) -> (int, bool) {
	if end - start == 1 {
		logical, logical_ok := keycode_from_name(combo[start:end])
		if logical_ok {
			return linux_logical_keycode_to_evdev(logical)
		}
	}
	if linux_combo_token_equals(combo, start, end, "home") {
		return LINUX_KEY_HOME, true
	}
	if linux_combo_token_equals(combo, start, end, "end") {
		return LINUX_KEY_END, true
	}
	if linux_combo_token_equals(combo, start, end, "page_up") || linux_combo_token_equals(combo, start, end, "pageup") {
		return LINUX_KEY_PAGEUP, true
	}
	if linux_combo_token_equals(combo, start, end, "page_down") || linux_combo_token_equals(combo, start, end, "pagedown") {
		return LINUX_KEY_PAGEDOWN, true
	}
	if linux_combo_token_equals(combo, start, end, "left") {
		return LINUX_KEY_LEFT, true
	}
	if linux_combo_token_equals(combo, start, end, "right") {
		return LINUX_KEY_RIGHT, true
	}
	if linux_combo_token_equals(combo, start, end, "down") {
		return LINUX_KEY_DOWN, true
	}
	if linux_combo_token_equals(combo, start, end, "up") {
		return LINUX_KEY_UP, true
	}
	if linux_combo_token_equals(combo, start, end, "tab") {
		return LINUX_KEY_TAB, true
	}
	if linux_combo_token_equals(combo, start, end, "return") || linux_combo_token_equals(combo, start, end, "enter") {
		return LINUX_KEY_ENTER, true
	}
	if linux_combo_token_equals(combo, start, end, "backspace") {
		return LINUX_KEY_BACKSPACE, true
	}
	if linux_combo_token_equals(combo, start, end, "delete") {
		return LINUX_KEY_DELETE, true
	}
	if linux_combo_token_equals(combo, start, end, "escape") || linux_combo_token_equals(combo, start, end, "esc") {
		return LINUX_KEY_ESC, true
	}
	return linux_combo_parse_function_key(combo, start, end)
}

linux_parse_combo_expression :: proc(combo: string, cursor: ^int, flags: ^int, keycode: ^int) -> bool {
	p := linux_combo_skip_ws(combo, cursor^)
	token_start := p
	for p < len(combo) && linux_combo_is_token_byte(combo[p]) {
		p += 1
	}
	if p == token_start {
		return false
	}
	token_end := p
	p = linux_combo_skip_ws(combo, p)
	if p < len(combo) && combo[p] == '(' {
		flag, flag_ok := linux_combo_modifier_flag(combo, token_start, token_end)
		if !flag_ok {
			return false
		}
		flags^ |= flag
		p += 1
		if !linux_parse_combo_expression(combo, &p, flags, keycode) {
			return false
		}
		p = linux_combo_skip_ws(combo, p)
		if p >= len(combo) || combo[p] != ')' {
			return false
		}
		cursor^ = p + 1
		return true
	}

	next_keycode, key_ok := linux_combo_keycode_from_token(combo, token_start, token_end)
	if !key_ok {
		return false
	}
	keycode^ = next_keycode
	cursor^ = p
	return true
}

linux_parse_key_combination :: proc(combo: string) -> (keycode: int, flags: int, ok: bool) {
	cursor := 0
	if !linux_parse_combo_expression(combo, &cursor, &flags, &keycode) {
		return 0, 0, false
	}
	cursor = linux_combo_skip_ws(combo, cursor)
	return keycode, flags, cursor == len(combo)
}

linux_emit_modifier :: proc(modifier: int, is_down: bool) -> bool {
	value := 0
	if is_down {
		value = 1
	}
	switch modifier {
	case LINUX_COMBO_SHIFT:
		return linux_uinput_emit_key_event(LINUX_KEY_LEFTSHIFT, value)
	case LINUX_COMBO_CONTROL:
		return linux_uinput_emit_key_event(LINUX_KEY_LEFTCTRL, value)
	case LINUX_COMBO_ALT:
		return linux_uinput_emit_key_event(LINUX_KEY_LEFTALT, value)
	case LINUX_COMBO_SUPER:
		return linux_uinput_emit_key_event(LINUX_KEY_LEFTMETA, value)
	}
	return true
}

linux_emit_key_combination :: proc(flags, keycode: int) -> bool {
	ok := true
	if (flags & LINUX_COMBO_SHIFT) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_SHIFT, true) && ok
	}
	if (flags & LINUX_COMBO_CONTROL) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_CONTROL, true) && ok
	}
	if (flags & LINUX_COMBO_ALT) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_ALT, true) && ok
	}
	if (flags & LINUX_COMBO_SUPER) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_SUPER, true) && ok
	}

	ok = linux_uinput_tap_key(keycode) && ok

	if (flags & LINUX_COMBO_SUPER) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_SUPER, false) && ok
	}
	if (flags & LINUX_COMBO_ALT) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_ALT, false) && ok
	}
	if (flags & LINUX_COMBO_CONTROL) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_CONTROL, false) && ok
	}
	if (flags & LINUX_COMBO_SHIFT) != 0 {
		ok = linux_emit_modifier(LINUX_COMBO_SHIFT, false) && ok
	}
	return ok
}

linux_send_key_combination :: proc(combo: string) -> bool {
	if !linux_output_init() {
		return false
	}
	keycode, flags, ok := linux_parse_key_combination(combo)
	if !ok {
		fmt.eprintln("stoin: unsupported key combo", combo)
		return false
	}
	linux_report_translation_timing_before_output("key-combo")
	return linux_emit_key_combination(flags, keycode)
}

linux_runtime_send_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return linux_send_text_utf8(text)
}

linux_runtime_delete_text :: proc(text: string, userdata: rawptr) -> bool {
	_ = userdata
	return linux_delete_text_utf8(text)
}

linux_runtime_send_key_combination :: proc(combo: string, userdata: rawptr) -> bool {
	_ = userdata
	return linux_send_key_combination(combo)
}
