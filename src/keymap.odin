package stoin

import "core:os"

Key_Binding :: struct {
	keycode: u16,
	bits:    u64,
}

Keymap :: struct {
	bindings: [dynamic]Key_Binding,
}

keymap_init :: proc(keymap: ^Keymap) {
	keymap^ = {}
	keymap.bindings = make([dynamic]Key_Binding)
}

keymap_destroy :: proc(keymap: ^Keymap) {
	if keymap == nil {
		return
	}
	delete(keymap.bindings)
	keymap^ = {}
}

keymap_is_space :: proc(c: byte) -> bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
}

keymap_ascii_to_lower :: proc(c: byte) -> byte {
	if c >= 'A' && c <= 'Z' {
		return c + ('a' - 'A')
	}
	return c
}

keymap_ascii_equal_ignore_case :: proc(a: string, b: string) -> bool {
	if len(a) != len(b) {
		return false
	}
	for i in 0..<len(a) {
		if keymap_ascii_to_lower(a[i]) != keymap_ascii_to_lower(b[i]) {
			return false
		}
	}
	return true
}

keycode_from_name :: proc(name: string) -> (u16, bool) {
	if len(name) == 1 {
		switch keymap_ascii_to_lower(name[0]) {
		case 'a': return 0, true
		case 's': return 1, true
		case 'd': return 2, true
		case 'f': return 3, true
		case 'h': return 4, true
		case 'g': return 5, true
		case 'z': return 6, true
		case 'x': return 7, true
		case 'c': return 8, true
		case 'v': return 9, true
		case 'b': return 11, true
		case 'q': return 12, true
		case 'w': return 13, true
		case 'e': return 14, true
		case 'r': return 15, true
		case 'y': return 16, true
		case 't': return 17, true
		case '1': return 18, true
		case '2': return 19, true
		case '3': return 20, true
		case '4': return 21, true
		case '6': return 22, true
		case '5': return 23, true
		case '9': return 25, true
		case '7': return 26, true
		case '8': return 28, true
		case '0': return 29, true
		case ']': return 30, true
		case 'o': return 31, true
		case 'u': return 32, true
		case '[': return 33, true
		case 'i': return 34, true
		case 'p': return 35, true
		case 'l': return 37, true
		case 'j': return 38, true
		case '\'': return 39, true
		case 'k': return 40, true
		case ';': return 41, true
		case '\\': return 42, true
		case ',': return 43, true
		case '/': return 44, true
		case 'n': return 45, true
		case 'm': return 46, true
		case '.': return 47, true
		case '`': return 50, true
		}
		return 0, false
	}

	if keymap_ascii_equal_ignore_case(name, "space") {
		return 49, true
	}
	if keymap_ascii_equal_ignore_case(name, "tab") {
		return 48, true
	}
	if keymap_ascii_equal_ignore_case(name, "enter") || keymap_ascii_equal_ignore_case(name, "return") {
		return 36, true
	}
	if keymap_ascii_equal_ignore_case(name, "escape") || keymap_ascii_equal_ignore_case(name, "esc") {
		return 53, true
	}
	if keymap_ascii_equal_ignore_case(name, "backspace") {
		return 51, true
	}
	if keymap_ascii_equal_ignore_case(name, "semicolon") {
		return 41, true
	}
	if keymap_ascii_equal_ignore_case(name, "apostrophe") || keymap_ascii_equal_ignore_case(name, "quote") {
		return 39, true
	}
	if keymap_ascii_equal_ignore_case(name, "comma") {
		return 43, true
	}
	if keymap_ascii_equal_ignore_case(name, "period") || keymap_ascii_equal_ignore_case(name, "dot") {
		return 47, true
	}
	if keymap_ascii_equal_ignore_case(name, "slash") {
		return 44, true
	}
	if keymap_ascii_equal_ignore_case(name, "backslash") {
		return 42, true
	}
	if keymap_ascii_equal_ignore_case(name, "right_shift") {
		return 60, true
	}
	if keymap_ascii_equal_ignore_case(name, "left_shift") {
		return 56, true
	}

	f_keys := [?]struct{name: string, keycode: u16} {
		{"f1", 122}, {"f2", 120}, {"f3", 99}, {"f4", 118},
		{"f5", 96}, {"f6", 97}, {"f7", 98}, {"f8", 100},
		{"f9", 101}, {"f10", 109}, {"f11", 103}, {"f12", 111},
		{"f13", 105}, {"f14", 107}, {"f15", 113}, {"f16", 106},
		{"f17", 64}, {"f18", 79}, {"f19", 80}, {"f20", 90},
	}
	for item in f_keys {
		if keymap_ascii_equal_ignore_case(name, item.name) {
			return item.keycode, true
		}
	}
	return 0, false
}

keymap_token :: proc(line: string, start: int) -> (token: string, next: int, ok: bool) {
	index := start
	for index < len(line) && keymap_is_space(line[index]) {
		index += 1
	}
	if index >= len(line) {
		return "", index, false
	}
	end := index
	for end < len(line) && !keymap_is_space(line[end]) {
		end += 1
	}
	return line[index:end], end, true
}

keymap_load :: proc(keymap: ^Keymap, path: string) -> bool {
	if keymap == nil || len(path) == 0 {
		return false
	}

	data, read_err := os.read_entire_file(path, context.allocator)
	if read_err != nil {
		return false
	}
	defer delete(data)

	next: Keymap
	keymap_init(&next)
	for line_start := 0; line_start <= len(data); {
		line_end := line_start
		for line_end < len(data) && data[line_end] != '\n' {
			line_end += 1
		}
		line := string(data[line_start:line_end])

		trimmed_start := 0
		for trimmed_start < len(line) && keymap_is_space(line[trimmed_start]) {
			trimmed_start += 1
		}
		trimmed := line[trimmed_start:]
		if len(trimmed) > 0 && !(len(trimmed) >= 2 && trimmed[:2] == "//") {
			key_name, next_index, key_ok := keymap_token(trimmed, 0)
			steno_name, _, steno_ok := keymap_token(trimmed, next_index)
			keycode, keycode_ok := keycode_from_name(key_name)
			bits, bits_ok := stroke_string_to_bits(steno_name)
			if !key_ok || !steno_ok || !keycode_ok || !bits_ok {
				keymap_destroy(&next)
				return false
			}
			append(&next.bindings, Key_Binding{keycode = keycode, bits = bits})
		}

		if line_end == len(data) {
			break
		}
		line_start = line_end + 1
	}

	if len(next.bindings) == 0 {
		keymap_destroy(&next)
		return false
	}
	keymap_destroy(keymap)
	keymap^ = next
	return true
}

keymap_find_binding :: proc(keymap: ^Keymap, keycode: u16) -> (binding: ^Key_Binding) {
	if keymap == nil {
		return nil
	}
	for i in 0..<len(keymap.bindings) {
		if keymap.bindings[i].keycode == keycode {
			return &keymap.bindings[i]
		}
	}
	return nil
}

keymap_binding_count :: proc(keymap: ^Keymap) -> int {
	if keymap == nil {
		return 0
	}
	return len(keymap.bindings)
}
