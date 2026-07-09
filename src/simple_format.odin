package stoin

import "core:strings"

Retro_Command :: enum {
	None,
	Toggle_Asterisk,
	Delete_Space,
	Insert_Space,
}

Case_Mode :: enum {
	Normal,
	Cap_First_Word,
	Upper_First_Word,
	Lower_First_Char,
	Upper,
	Lower,
	Title,
}

Formatted_Text :: struct {
	text:         string,
	ortho_suffix: string,
	ortho_suffix_text_offset: int,
	ortho_suffix_text_length: int,
	attach_prev:  bool,
	attach_next:  bool,
	glue:         bool,
	stitch:       bool,
	stitch_last_word: bool,
	stitch_count: int,
	stitch_delimiter: string,
	key_combos: [dynamic]string,
	retro_command: Retro_Command,
	text_case: Case_Mode,
	next_case: Case_Mode,
	retro_case: Case_Mode,
	mode_command: string,
	plover_command: string,
	carry_case: bool,
	cancel_formatting: bool,
}

formatted_text_destroy :: proc(formatted: ^Formatted_Text) {
	delete(formatted.text)
	owned_string_delete(formatted.ortho_suffix)
	owned_string_delete(formatted.stitch_delimiter)
	for combo in formatted.key_combos {
		owned_string_delete(combo)
	}
	delete(formatted.key_combos)
	owned_string_delete(formatted.mode_command)
	owned_string_delete(formatted.plover_command)
	formatted^ = {}
}

formatted_append_bytes :: proc(buffer: ^[dynamic]byte, data: []byte) -> bool {
	for b in data {
		append(buffer, b)
	}
	return true
}

formatted_append_string :: proc(buffer: ^[dynamic]byte, text: string) -> bool {
	return formatted_append_bytes(buffer, transmute([]byte)text)
}

formatted_prepend_string :: proc(formatted: ^Formatted_Text, prefix: string) -> bool {
	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, prefix)
	formatted_append_string(&buffer, formatted.text)

	text, clone_err := strings.clone(string(buffer[:]))
	if clone_err != nil {
		return false
	}
	delete(formatted.text)
	formatted.text = text
	return true
}

parse_positive_int :: proc(text: string) -> (value: int, ok: bool) {
	if len(text) == 0 {
		return 0, false
	}
	for i in 0..<len(text) {
		if text[i] < '0' || text[i] > '9' {
			return 0, false
		}
		value = value * 10 + int(text[i] - '0')
	}
	return value, value > 0
}

formatted_is_digit_string :: proc(text: string) -> bool {
	if len(text) == 0 {
		return false
	}
	for c in text {
		if c < '0' || c > '9' {
			return false
		}
	}
	return true
}

formatted_set_stitch_delimiter :: proc(formatted: ^Formatted_Text, delimiter: string) -> bool {
	copy, copy_ok := clone_string_ok(delimiter)
	if !copy_ok {
		return false
	}
	owned_string_delete(formatted.stitch_delimiter)
	formatted.stitch_delimiter = copy
	return true
}

formatted_stitch_delimiter :: proc(formatted: ^Formatted_Text) -> string {
	if len(formatted.stitch_delimiter) == 0 {
		return "-"
	}
	return formatted.stitch_delimiter
}

formatted_parse_stitch_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, meta: string) -> bool {
	if len(meta) >= 8 && meta[:8] == ":stitch:" {
		args := meta[8:]
		separator := -1
		for i in 0..<len(args) {
			if args[i] == ':' {
				separator = i
				break
			}
		}

		word := args
		delimiter := "-"
		if separator >= 0 {
			word = args[:separator]
			delimiter = args[separator + 1:]
		}
		if len(word) == 0 {
			return false
		}

		formatted.stitch = true
		formatted.glue = true
		return formatted_set_stitch_delimiter(formatted, delimiter) &&
			formatted_append_string(buffer, word)
	}

	command := ":stitch_last_word"
	if len(meta) < len(command) || meta[:len(command)] != command {
		return false
	}
	if len(meta) > len(command) && meta[len(command)] != ':' {
		return false
	}

	count := 1
	delimiter := "-"
	args := ""
	if len(meta) > len(command) {
		args = meta[len(command) + 1:]
		separator := -1
		for i in 0..<len(args) {
			if args[i] == ':' {
				separator = i
				break
			}
		}
		count_text := args
		if separator >= 0 {
			count_text = args[:separator]
			delimiter = args[separator + 1:]
		}
		if len(count_text) > 0 {
			parsed_count, parsed := parse_positive_int(count_text)
			if !parsed {
				return false
			}
			count = parsed_count
		}
	}

	formatted.stitch_last_word = true
	formatted.stitch_count = count
	return formatted_set_stitch_delimiter(formatted, delimiter)
}

formatted_parse_case_name :: proc(name: string) -> (Case_Mode, bool) {
	if formatted_ascii_equal_ignore_case(name, "cap_first_word") {
		return .Cap_First_Word, true
	}
	if formatted_ascii_equal_ignore_case(name, "upper_first_word") {
		return .Upper_First_Word, true
	}
	if formatted_ascii_equal_ignore_case(name, "lower_first_char") {
		return .Lower_First_Char, true
	}
	return .Normal, false
}

formatted_ascii_to_lower :: proc(c: byte) -> byte {
	if c >= 'A' && c <= 'Z' {
		return c + ('a' - 'A')
	}
	return c
}

formatted_ascii_equal_ignore_case :: proc(a: string, b: string) -> bool {
	if len(a) != len(b) {
		return false
	}
	for i in 0..<len(a) {
		if formatted_ascii_to_lower(a[i]) != formatted_ascii_to_lower(b[i]) {
			return false
		}
	}
	return true
}

formatted_parse_case_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, meta: string) -> bool {
	if len(meta) == 1 && meta[0] == '>' {
		if len(buffer^) == 0 && formatted.text_case == .Normal {
			formatted.text_case = .Lower_First_Char
		} else {
			formatted.next_case = .Lower_First_Char
		}
		return true
	}
	if len(meta) == 1 && meta[0] == '<' {
		if len(buffer^) == 0 && formatted.text_case == .Normal {
			formatted.text_case = .Upper_First_Word
		} else {
			formatted.next_case = .Upper_First_Word
		}
		return true
	}
	if len(meta) == 2 && meta[0] == '-' && meta[1] == '|' {
		if len(buffer^) == 0 && formatted.text_case == .Normal {
			formatted.text_case = .Cap_First_Word
		} else {
			formatted.next_case = .Cap_First_Word
		}
		formatted.attach_next = true
		return true
	}
	if len(meta) > 6 && formatted_ascii_equal_ignore_case(meta[:6], ":case:") {
		mode, ok := formatted_parse_case_name(meta[6:])
		if !ok {
			return false
		}
		if len(buffer^) == 0 && formatted.text_case == .Normal {
			formatted.text_case = mode
		} else {
			formatted.next_case = mode
		}
		return true
	}
	if len(meta) > 12 && formatted_ascii_equal_ignore_case(meta[:12], ":retro_case:") {
		mode, ok := formatted_parse_case_name(meta[12:])
		if !ok {
			return false
		}
		formatted.retro_case = mode
		return true
	}
	if len(meta) == 3 && meta[0] == '*' && meta[1] == '-' && meta[2] == '|' {
		formatted.retro_case = .Cap_First_Word
		return true
	}
	if len(meta) == 2 && meta[0] == '*' && meta[1] == '<' {
		formatted.retro_case = .Upper_First_Word
		return true
	}
	if len(meta) == 2 && meta[0] == '*' && meta[1] == '>' {
		formatted.retro_case = .Lower_First_Char
		return true
	}
	return false
}

formatted_parse_mode_meta :: proc(formatted: ^Formatted_Text, meta: string) -> bool {
	if len(meta) < 5 || !formatted_ascii_equal_ignore_case(meta[:5], "MODE:") {
		return false
	}
	command, ok := clone_string_ok(meta[5:])
	if !ok {
		return false
	}
	owned_string_delete(formatted.mode_command)
	formatted.mode_command = command
	return true
}

formatted_parse_plover_command_meta :: proc(formatted: ^Formatted_Text, meta: string) -> bool {
	if len(meta) < 7 || !formatted_ascii_equal_ignore_case(meta[:7], "PLOVER:") {
		return false
	}
	command, ok := clone_string_ok(meta[7:])
	if !ok {
		return false
	}
	owned_string_delete(formatted.plover_command)
	formatted.plover_command = command
	return true
}

formatted_parse_key_combo_meta :: proc(formatted: ^Formatted_Text, meta: string) -> bool {
	if len(meta) < 2 || meta[0] != '#' {
		return false
	}
	combo, ok := clone_string_ok(meta[1:])
	if !ok {
		return false
	}
	if formatted.key_combos == nil {
		formatted.key_combos = make([dynamic]string)
	}
	append(&formatted.key_combos, combo)
	return true
}

formatted_parse_carry_case_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, meta: string) -> bool {
	separator := -1
	for i := 0; i + 1 < len(meta); i += 1 {
		if meta[i] == '~' && meta[i + 1] == '|' {
			separator = i
			break
		}
	}
	if separator < 0 {
		return false
	}

	begin := len(meta) > 0 && meta[0] == '^'
	end := len(meta) > 0 && meta[len(meta) - 1] == '^'
	start := separator + 2
	end_index := len(meta)
	if end {
		end_index -= 1
	}
	if start > end_index {
		start = end_index
	}

	if begin {
		formatted.attach_prev = true
	}
	if end {
		formatted.attach_next = true
	}
	formatted.carry_case = true
	return formatted_append_string(buffer, meta[start:end_index])
}

formatted_parse_attach_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, meta: string, pending_attach_prev: ^bool) -> bool {
	begin := len(meta) > 0 && meta[0] == '^'
	end := len(meta) > 0 && meta[len(meta) - 1] == '^'
	if !begin && !end {
		begin = true
		end = true
	}

	start := 0
	if begin {
		start = 1
		formatted.attach_prev = true
	}
	end_index := len(meta)
	if end {
		end_index -= 1
		formatted.attach_next = true
	}
	if start > end_index {
		start = end_index
	}

	if start == end_index {
		if begin {
			pending_attach_prev^ = true
		}
		return true
	}

	if pending_attach_prev^ {
		formatted.attach_prev = true
		pending_attach_prev^ = false
	}
	if begin && !end && len(formatted.ortho_suffix) == 0 {
		suffix, suffix_ok := clone_string_ok(meta[start:end_index])
		if !suffix_ok {
			return false
		}
		formatted.ortho_suffix = suffix
		formatted.ortho_suffix_text_offset = len(buffer^)
		formatted.ortho_suffix_text_length = end_index - start
	}
	return formatted_append_string(buffer, meta[start:end_index])
}

formatted_apply_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, translation: string, meta: string, pending_attach_prev: ^bool) -> bool {
	if len(meta) == 0 {
		formatted.cancel_formatting = true
		formatted.text_case = .Normal
		formatted.next_case = .Normal
		return true
	}

	if len(meta) == 1 {
		switch meta[0] {
		case '.', ',', ':', ';', '?', '!':
			formatted.attach_prev = true
			append(buffer, meta[0])
			if meta[0] == '.' || meta[0] == '?' || meta[0] == '!' {
				formatted.next_case = .Cap_First_Word
			}
			return true
		case '#':
			return true
		}
	}

	if formatted_parse_stitch_meta(formatted, buffer, meta) {
		return true
	}

	if formatted_parse_case_meta(formatted, buffer, meta) {
		return true
	}

	if formatted_parse_key_combo_meta(formatted, meta) {
		return true
	}

	if formatted_parse_plover_command_meta(formatted, meta) {
		return true
	}

	if formatted_parse_mode_meta(formatted, meta) {
		return true
	}

	if formatted_parse_carry_case_meta(formatted, buffer, meta) {
		return true
	}

	if meta[0] == '^' || meta[len(meta) - 1] == '^' {
		return formatted_parse_attach_meta(formatted, buffer, meta, pending_attach_prev)
	}

	if len(meta) == 1 && meta[0] == '*' {
		formatted.retro_command = .Toggle_Asterisk
		return true
	}
	if len(meta) == 2 && meta[0] == '*' && meta[1] == '!' {
		formatted.retro_command = .Delete_Space
		return true
	}
	if len(meta) == 2 && meta[0] == '*' && meta[1] == '?' {
		formatted.retro_command = .Insert_Space
		return true
	}

	if len(meta) >= 5 && meta[:5] == "glue:" {
		formatted.glue = true
		return formatted_append_string(buffer, meta[5:])
	}
	if len(meta) >= 6 && meta[:6] == ":glue:" {
		formatted.glue = true
		return formatted_append_string(buffer, meta[6:])
	}
	if meta[0] == '&' {
		formatted.glue = true
		return formatted_append_string(buffer, meta[1:])
	}
	if meta[0] == '#' {
		return true
	}

	if pending_attach_prev^ {
		formatted.attach_prev = true
		pending_attach_prev^ = false
	}
	return formatted_append_string(buffer, translation)
}

format_translation_text_basic :: proc(translation: string) -> (formatted: Formatted_Text, ok: bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	if formatted_is_digit_string(translation) {
		formatted.glue = true
	}

	pending_attach_prev := false
	for i := 0; i < len(translation); {
		c := translation[i]
		if c == '\\' {
			if pending_attach_prev {
				formatted.attach_prev = true
				pending_attach_prev = false
			}
			i += 1
			if i >= len(translation) {
				append(&buffer, '\\')
				break
			}
			switch translation[i] {
			case 'n':
				append(&buffer, '\n')
			case 'r':
				append(&buffer, '\r')
			case 't':
				append(&buffer, '\t')
			case:
				append(&buffer, translation[i])
			}
			i += 1
			continue
		}

		if c != '{' {
			if pending_attach_prev {
				formatted.attach_prev = true
				pending_attach_prev = false
			}
			append(&buffer, c)
			i += 1
			continue
		}

		meta_start := i + 1
		meta_end := meta_start
		for meta_end < len(translation) {
			if translation[meta_end] == '\\' && meta_end + 1 < len(translation) {
				meta_end += 2
			} else if translation[meta_end] == '}' {
				break
			} else {
				meta_end += 1
			}
		}

		if meta_end >= len(translation) || translation[meta_end] != '}' {
			append(&buffer, c)
			i += 1
			continue
		}

		if !formatted_apply_meta(&formatted, &buffer, translation[i:meta_end + 1], translation[meta_start:meta_end], &pending_attach_prev) {
			formatted_text_destroy(&formatted)
			return {}, false
		}
		i = meta_end + 1
	}

	if pending_attach_prev {
		formatted.attach_prev = true
		formatted.attach_next = true
	}
	if len(buffer) == 0 && formatted.text_case != .Normal {
		formatted.next_case = formatted.text_case
		formatted.text_case = .Normal
	}

	text, clone_err := strings.clone(string(buffer[:]))
	if clone_err != nil {
		return {}, false
	}
	formatted.text = text
	return formatted, true
}
