package stoin

import "base:runtime"

TRANSLATION_MATCH_MAX_STROKES :: 100

Applied_Translation :: struct {
	strokes:      [dynamic]u64,
	text:         string,
	next_attach:  bool,
	glue:         bool,
	split_prefix_text: string,
	split_prefix_stroke_count: int,
	replaced:     [dynamic]Applied_Translation,
	retro_space_command: bool,
	previous_case_mode: Case_Mode,
	previous_next_case: Case_Mode,
	has_case_state: bool,
}

Translation_Match :: struct {
	translation:    string,
	suffix_base_translation: string,
	suffix_translation: string,
	suffix_match:    bool,
	strokes:         [dynamic]u64,
	replaced_count:  int,
	outline:         string,
	partial_prefix_text: string,
	partial_prefix_stroke_count: int,
	found:           bool,
}

Simple_Engine :: struct {
	dictionary: ^Dictionary,
	dictionary_stack: ^Dictionary_Stack,
	orthography: ^Orthography,
	history:    [dynamic]Applied_Translation,
	key_combos: [dynamic]string,
	case_mode:  Case_Mode,
	next_case:  Case_Mode,
	spacing:    string,
}

simple_engine_init :: proc(engine: ^Simple_Engine, dictionary: ^Dictionary) {
	engine^ = {}
	engine.dictionary = dictionary
	engine.history = make([dynamic]Applied_Translation)
	engine.key_combos = make([dynamic]string)
	engine.spacing, _ = clone_string_ok(" ")
}

simple_engine_init_with_stack :: proc(engine: ^Simple_Engine, stack: ^Dictionary_Stack) {
	simple_engine_init(engine, &stack.dictionary)
	engine.dictionary_stack = stack
}

simple_engine_set_orthography :: proc(engine: ^Simple_Engine, orthography: ^Orthography) {
	engine.orthography = orthography
}

simple_engine_destroy :: proc(engine: ^Simple_Engine) {
	for i in 0..<len(engine.history) {
		applied_translation_destroy(&engine.history[i])
	}
	delete(engine.history)
	for combo in engine.key_combos {
		owned_string_delete(combo)
	}
	delete(engine.key_combos)
	owned_string_delete(engine.spacing)
	engine^ = {}
}

simple_engine_set_spacing :: proc(engine: ^Simple_Engine, spacing: string) -> bool {
	copy, ok := clone_string_ok(spacing)
	if !ok {
		return false
	}
	owned_string_delete(engine.spacing)
	engine.spacing = copy
	return true
}

applied_translation_destroy :: proc(translation: ^Applied_Translation) {
	delete(translation.strokes)
	owned_string_delete(translation.text)
	owned_string_delete(translation.split_prefix_text)
	for i in 0..<len(translation.replaced) {
		applied_translation_destroy(&translation.replaced[i])
	}
	delete(translation.replaced)
	translation^ = {}
}

applied_translation_destroy_without_replaced :: proc(translation: ^Applied_Translation) {
	delete(translation.strokes)
	owned_string_delete(translation.text)
	owned_string_delete(translation.split_prefix_text)
	delete(translation.replaced)
	translation^ = {}
}

translation_match_destroy :: proc(match: ^Translation_Match) {
	delete(match.strokes)
	owned_string_delete(match.outline)
	owned_string_delete(match.partial_prefix_text)
	match^ = {}
}

translation_match_clear_partial_prefix :: proc(match: ^Translation_Match) {
	owned_string_delete(match.partial_prefix_text)
	match.partial_prefix_text = ""
	match.partial_prefix_stroke_count = 0
}

translation_match_set_partial_prefix :: proc(match: ^Translation_Match, text: string, stroke_count: int) -> bool {
	translation_match_clear_partial_prefix(match)
	if len(text) == 0 {
		match.partial_prefix_stroke_count = stroke_count
		return true
	}
	copy, ok := clone_string_ok(text)
	if !ok {
		return false
	}
	match.partial_prefix_text = copy
	match.partial_prefix_stroke_count = stroke_count
	return true
}

applied_translation_set_split_prefix :: proc(translation: ^Applied_Translation, text: string, stroke_count: int) -> bool {
	owned_string_delete(translation.split_prefix_text)
	translation.split_prefix_text = ""
	translation.split_prefix_stroke_count = 0

	if stroke_count <= 0 {
		return true
	}
	if len(text) == 0 {
		translation.split_prefix_stroke_count = stroke_count
		return true
	}
	copy, ok := clone_string_ok(text)
	if !ok {
		return false
	}
	translation.split_prefix_text = copy
	translation.split_prefix_stroke_count = stroke_count
	return true
}

clone_bytes_to_string :: proc(data: []byte) -> (string, bool) {
	cloned := make([]byte, len(data), runtime.heap_allocator())
	if len(data) > 0 && len(cloned) != len(data) {
		return "", false
	}
	copy(cloned, data)
	return string(cloned), true
}

clone_string_ok :: proc(s: string) -> (string, bool) {
	return clone_bytes_to_string(transmute([]byte)s)
}

owned_string_delete :: proc(text: string) {
	if len(text) != 0 {
		delete(text, runtime.heap_allocator())
	}
}

stroke_sequence_to_string_alloc :: proc(strokes: []u64) -> (outline: string, ok: bool) {
	buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
	n, formatted := strokes_to_outline_key(strokes, buffer[:])
	if !formatted {
		return "", false
	}
	return clone_bytes_to_string(buffer[:n])
}
