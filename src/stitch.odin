package stoin

Text_Token :: struct {
	start:    int,
	core_end: int,
	end:      int,
}

stitch_is_ascii_space :: proc(c: byte) -> bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
}

stitch_is_word_byte :: proc(c: byte) -> bool {
	return c >= 0x80 ||
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		c == '_' || c == '\''
}

stitch_utf8_codepoint_length :: proc(text: string, index: int, end: int) -> int {
	if index >= end {
		return 0
	}
	c := text[index]
	length := 1
	if (c & 0x80) == 0 {
		length = 1
	} else if (c & 0xE0) == 0xC0 {
		length = 2
	} else if (c & 0xF0) == 0xE0 {
		length = 3
	} else if (c & 0xF8) == 0xF0 {
		length = 4
	}
	if index + length > end {
		return end - index
	}
	return length
}

stitch_collect_text_tokens :: proc(text: string) -> (tokens: [dynamic]Text_Token, ok: bool) {
	tokens = make([dynamic]Text_Token)
	index := 0
	for index < len(text) {
		for index < len(text) && stitch_is_ascii_space(text[index]) {
			index += 1
		}
		if index >= len(text) {
			break
		}

		start := index
		if stitch_is_word_byte(text[index]) {
			index += stitch_utf8_codepoint_length(text, index, len(text))
			for index < len(text) {
				c := text[index]
				if stitch_is_word_byte(c) || c == '-' {
					index += stitch_utf8_codepoint_length(text, index, len(text))
				} else {
					break
				}
			}
		} else {
			index += stitch_utf8_codepoint_length(text, index, len(text))
			for index < len(text) && !stitch_is_ascii_space(text[index]) && !stitch_is_word_byte(text[index]) {
				index += stitch_utf8_codepoint_length(text, index, len(text))
			}
		}

		core_end := index
		for index < len(text) && stitch_is_ascii_space(text[index]) {
			index += 1
		}
		append(&tokens, Text_Token{start = start, core_end = core_end, end = index})
	}
	return tokens, true
}

stitch_append_stitched_core :: proc(buffer: ^[dynamic]byte, text: string, start: int, end: int, delimiter: string) -> bool {
	first := true
	for index := start; index < end; {
		length := stitch_utf8_codepoint_length(text, index, end)
		if length <= 0 {
			return false
		}
		if !first && !formatted_append_string(buffer, delimiter) {
			return false
		}
		if !formatted_append_string(buffer, text[index:index + length]) {
			return false
		}
		index += length
		first = false
	}
	return true
}

stitch_text_suffix :: proc(text: string, token_count: int, delimiter: string) -> (string, bool) {
	tokens, tokens_ok := stitch_collect_text_tokens(text)
	defer delete(tokens)
	if !tokens_ok {
		return "", false
	}
	if len(tokens) == 0 || token_count == 0 {
		return clone_string_ok(text)
	}
	actual_count := token_count
	if actual_count > len(tokens) {
		actual_count = len(tokens)
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)

	first_token := len(tokens) - actual_count
	prefix_end := tokens[first_token].start
	formatted_append_string(&buffer, text[:prefix_end])
	for i in first_token..<len(tokens) {
		token := tokens[i]
		if !stitch_append_stitched_core(&buffer, text, token.start, token.core_end, delimiter) {
			return "", false
		}
		formatted_append_string(&buffer, text[token.core_end:token.end])
	}
	return clone_bytes_to_string(buffer[:])
}

stitch_token_count :: proc(text: string) -> int {
	tokens, tokens_ok := stitch_collect_text_tokens(text)
	defer delete(tokens)
	if !tokens_ok {
		return 0
	}
	return len(tokens)
}
