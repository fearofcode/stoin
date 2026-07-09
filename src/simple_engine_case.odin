package stoin

import "core:strings"

simple_engine_is_word_byte :: proc(c: byte) -> bool {
	return c >= 0x80 ||
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		c == '_'
}

simple_engine_is_alpha :: proc(c: byte) -> bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
}

simple_engine_to_upper :: proc(c: byte) -> byte {
	if c >= 'a' && c <= 'z' {
		return c - ('a' - 'A')
	}
	return c
}

simple_engine_to_lower :: proc(c: byte) -> byte {
	if c >= 'A' && c <= 'Z' {
		return c + ('a' - 'A')
	}
	return c
}

simple_engine_apply_case_to_text :: proc(text: string, mode: Case_Mode) -> (string, bool) {
	if mode == .Normal || len(text) == 0 {
		copied, clone_err := strings.clone(text)
		if clone_err != nil {
			return "", false
		}
		return copied, true
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, text)

	switch mode {
	case .Upper:
		for i in 0..<len(buffer) {
			buffer[i] = simple_engine_to_upper(buffer[i])
		}
	case .Lower:
		for i in 0..<len(buffer) {
			buffer[i] = simple_engine_to_lower(buffer[i])
		}
	case .Title:
		in_word := false
		for i in 0..<len(buffer) {
			c := buffer[i]
			if !simple_engine_is_word_byte(c) {
				in_word = false
				continue
			}
			buffer[i] = in_word ? simple_engine_to_lower(c) : simple_engine_to_upper(c)
			in_word = true
		}
	case .Upper_First_Word:
		in_word := false
		for i in 0..<len(buffer) {
			c := buffer[i]
			if !in_word {
				if !simple_engine_is_word_byte(c) {
					continue
				}
				in_word = true
			} else if !simple_engine_is_word_byte(c) {
				break
			}
			buffer[i] = simple_engine_to_upper(c)
		}
	case .Cap_First_Word:
		for i in 0..<len(buffer) {
			if simple_engine_is_alpha(buffer[i]) {
				buffer[i] = simple_engine_to_upper(buffer[i])
				break
			}
		}
	case .Lower_First_Char:
		for i in 0..<len(buffer) {
			if simple_engine_is_alpha(buffer[i]) {
				buffer[i] = simple_engine_to_lower(buffer[i])
				break
			}
		}
	case .Normal:
	}

	cased, clone_err := strings.clone(string(buffer[:]))
	if clone_err != nil {
		return "", false
	}
	return cased, true
}

simple_engine_replace_formatted_text :: proc(formatted: ^Formatted_Text, text: string) {
	delete(formatted.text)
	formatted.text = text
}

simple_engine_apply_case_state_to_formatted :: proc(engine: ^Simple_Engine, formatted: ^Formatted_Text) -> bool {
	if formatted.cancel_formatting {
		engine.next_case = .Normal
	}
	if formatted.carry_case && formatted.next_case == .Normal && engine.next_case != .Normal {
		formatted.next_case = engine.next_case
	}
	if len(formatted.text) > 0 {
		cased, case_ok := simple_engine_apply_case_to_text(formatted.text, engine.case_mode)
		if !case_ok {
			return false
		}
		simple_engine_replace_formatted_text(formatted, cased)

		text_case := engine.next_case
		if formatted.text_case != .Normal {
			text_case = formatted.text_case
		}
		cased, case_ok = simple_engine_apply_case_to_text(formatted.text, text_case)
		if !case_ok {
			return false
		}
		simple_engine_replace_formatted_text(formatted, cased)
		engine.next_case = formatted.next_case
	} else if formatted.next_case != .Normal {
		engine.next_case = formatted.next_case
	}
	return true
}

