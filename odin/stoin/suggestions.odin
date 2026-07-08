package stoin

BREVITY_MAX_TRANSLATIONS :: 5

Brevity_Suggestion :: struct {
	suggested_outline: string,
	typed_outline:     string,
	text:              string,
	typed_strokes:     int,
	suggested_strokes: int,
	saved_strokes:     int,
}

brevity_suggestion_destroy :: proc(suggestion: ^Brevity_Suggestion) {
	owned_string_delete(suggestion.suggested_outline)
	owned_string_delete(suggestion.typed_outline)
	owned_string_delete(suggestion.text)
	suggestion^ = {}
}

skip_leading_ascii_space :: proc(text: string) -> string {
	index := 0
	for index < len(text) {
		switch text[index] {
		case ' ', '\t', '\n', '\r', '\v', '\f':
			index += 1
		case:
			return text[index:]
		}
	}
	return text[index:]
}

brevity_append_translation_outline :: proc(buffer: ^[dynamic]byte, translation: ^Applied_Translation, has_stroke: ^bool, stroke_count: ^int) -> bool {
	for stroke in translation.strokes {
		stroke_buffer: [64]byte
		stroke_len, stroke_ok := chord_bits_to_string(stroke, stroke_buffer[:])
		if !stroke_ok {
			return false
		}
		if has_stroke^ {
			append(buffer, '/')
		}
		if !formatted_append_bytes(buffer, stroke_buffer[:stroke_len]) {
			return false
		}
		has_stroke^ = true
		stroke_count^ += 1
	}
	return true
}

brevity_build_candidate :: proc(engine: ^Simple_Engine, start: int, count: int) -> (text: string, typed_outline: string, typed_strokes: int, ok: bool) {
	text_buffer := make([dynamic]byte)
	defer delete(text_buffer)
	outline_buffer := make([dynamic]byte)
	defer delete(outline_buffer)

	has_stroke := false
	for i in start..<start + count {
		translation := &engine.history[i]
		if !formatted_append_string(&text_buffer, translation.text) {
			return "", "", 0, false
		}
		if !brevity_append_translation_outline(&outline_buffer, translation, &has_stroke, &typed_strokes) {
			return "", "", 0, false
		}
	}

	candidate_text := skip_leading_ascii_space(string(text_buffer[:]))
	if len(candidate_text) == 0 {
		return "", "", 0, false
	}

	text_clone, text_ok := clone_string_ok(candidate_text)
	if !text_ok {
		return "", "", 0, false
	}
	typed_outline_clone, outline_ok := clone_bytes_to_string(outline_buffer[:])
	if !outline_ok {
		owned_string_delete(text_clone)
		return "", "", 0, false
	}
	return text_clone, typed_outline_clone, typed_strokes, true
}

brevity_suggest :: proc(engine: ^Simple_Engine) -> (suggestion: Brevity_Suggestion, ok: bool) {
	if engine == nil || engine.dictionary == nil {
		return {}, false
	}

	translation_count := len(engine.history)
	max_window := translation_count
	if max_window > BREVITY_MAX_TRANSLATIONS {
		max_window = BREVITY_MAX_TRANSLATIONS
	}

	for window := max_window; window > 0; window -= 1 {
		start := translation_count - window
		text, typed_outline, typed_strokes, candidate_ok := brevity_build_candidate(engine, start, window)
		if !candidate_ok {
			continue
		}
		if typed_strokes <= 1 {
			owned_string_delete(text)
			owned_string_delete(typed_outline)
			continue
		}

		suggested_outline, found := dictionary_find_translation_outline(
			engine.dictionary,
			text,
			typed_outline,
			typed_strokes - 1,
		)
		if !found {
			owned_string_delete(text)
			owned_string_delete(typed_outline)
			continue
		}

		suggested_strokes := outline_key_stroke_count(suggested_outline)
		saved_strokes := 0
		if typed_strokes > suggested_strokes {
			saved_strokes = typed_strokes - suggested_strokes
		}
		return Brevity_Suggestion {
			suggested_outline = suggested_outline,
			typed_outline = typed_outline,
			text = text,
			typed_strokes = typed_strokes,
			suggested_strokes = suggested_strokes,
			saved_strokes = saved_strokes,
		}, true
	}

	return {}, false
}
