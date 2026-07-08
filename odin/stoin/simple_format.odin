package stoin

import "core:strings"

Formatted_Text :: struct {
	text:        string,
	attach_prev: bool,
	attach_next: bool,
	glue:        bool,
}

formatted_text_destroy :: proc(formatted: ^Formatted_Text) {
	delete(formatted.text)
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
	return formatted_append_string(buffer, meta[start:end_index])
}

formatted_apply_meta :: proc(formatted: ^Formatted_Text, buffer: ^[dynamic]byte, translation: string, meta: string, pending_attach_prev: ^bool) -> bool {
	if len(meta) == 0 {
		return true
	}

	if len(meta) == 1 {
		switch meta[0] {
		case '.', ',', ':', ';', '?', '!':
			formatted.attach_prev = true
			append(buffer, meta[0])
			return true
		case '#':
			return true
		}
	}

	if meta[0] == '^' || meta[len(meta) - 1] == '^' {
		return formatted_parse_attach_meta(formatted, buffer, meta, pending_attach_prev)
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
	if len(meta) >= 5 && (meta[:5] == "MODE:" || meta[:5] == "mode:") {
		return true
	}
	if len(meta) >= 7 && (meta[:7] == "PLOVER:" || meta[:7] == "plover:") {
		return true
	}
	if len(meta) >= 6 && meta[:6] == ":case:" {
		return true
	}
	if len(meta) >= 12 && meta[:12] == ":retro_case:" {
		return true
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

	text, clone_err := strings.clone(string(buffer[:]))
	if clone_err != nil {
		return {}, false
	}
	formatted.text = text
	return formatted, true
}
