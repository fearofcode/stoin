package stoin

import "core:time"

TRANSLATION_COMPACT_INTERVAL_STROKES :: 1000
TRANSLATION_HISTORY_STROKE_LIMIT :: 1000

Send_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Delete_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Send_Key_Combination_Callback :: proc(combo: string, userdata: rawptr) -> bool
Line_Output_Callback :: proc(line: string, userdata: rawptr) -> bool

Steno_Phrase_Mode :: enum {
	None,
	All,
	Verbs,
	Nonverbs,
}

Stroke_Input :: struct {
	bits:             u64,
	phrase:           bool,
	phrase_namespace: bool,
	phrase_mode:      Steno_Phrase_Mode,
}

Steno_Runtime_Config :: struct {
	dictionary:           ^Dictionary,
	dictionary_stack:     ^Dictionary_Stack,
	orthography:          ^Orthography,
	phrasing:             ^Phrasing,
	send_text:            Send_Text_Callback,
	delete_text:          Delete_Text_Callback,
	send_key_combination: Send_Key_Combination_Callback,
	write_trace:          Line_Output_Callback,
	write_suggestion:     Line_Output_Callback,
	write_suggestion_log: Line_Output_Callback,
	userdata:             rawptr,
}

Steno_Runtime :: struct {
	engine:                   Simple_Engine,
	phrasing:                 ^Phrasing,
	send_text:                Send_Text_Callback,
	delete_text:              Delete_Text_Callback,
	send_key_combination:     Send_Key_Combination_Callback,
	write_trace:              Line_Output_Callback,
	write_suggestion:         Line_Output_Callback,
	write_suggestion_log:     Line_Output_Callback,
	userdata:                 rawptr,
	session_active:           bool,
	phrase_namespace_enabled: bool,
	phrase_mode:              Steno_Phrase_Mode,
	strokes_since_compaction: int,
}

Trace_Stroke_Mode :: enum {
	Normal,
	Phrase,
	Phase_Fallback,
}

steno_runtime_init :: proc(runtime: ^Steno_Runtime, config: ^Steno_Runtime_Config) -> bool {
	if runtime == nil || config == nil || config.send_text == nil || config.delete_text == nil {
		return false
	}
	if config.dictionary_stack != nil {
		simple_engine_init_with_stack(&runtime.engine, config.dictionary_stack)
	} else if config.dictionary != nil {
		simple_engine_init(&runtime.engine, config.dictionary)
	} else {
		return false
	}

	if config.orthography != nil {
		simple_engine_set_orthography(&runtime.engine, config.orthography)
	}
	runtime.phrasing = config.phrasing
	runtime.send_text = config.send_text
	runtime.delete_text = config.delete_text
	runtime.send_key_combination = config.send_key_combination
	runtime.write_trace = config.write_trace
	runtime.write_suggestion = config.write_suggestion
	runtime.write_suggestion_log = config.write_suggestion_log
	runtime.userdata = config.userdata
	runtime.session_active = true
	return true
}

steno_runtime_destroy :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	simple_engine_destroy(&runtime.engine)
	runtime^ = {}
}

steno_runtime_set_session_active :: proc(runtime: ^Steno_Runtime, active: bool) {
	if runtime == nil {
		return
	}
	runtime.session_active = active
	if !active {
		runtime.phrase_mode = .None
	}
}

steno_runtime_set_phrase_namespace_enabled :: proc(runtime: ^Steno_Runtime, enabled: bool) {
	if runtime == nil {
		return
	}
	runtime.phrase_namespace_enabled = enabled
	if !enabled {
		runtime.phrase_mode = .None
	}
}

steno_runtime_set_phrase_mode :: proc(runtime: ^Steno_Runtime, mode: Steno_Phrase_Mode) {
	if runtime == nil {
		return
	}
	runtime.phrase_mode = mode
}

steno_runtime_translation_history_stroke_count :: proc(runtime: ^Steno_Runtime) -> int {
	if runtime == nil {
		return 0
	}
	return simple_engine_history_stroke_count(&runtime.engine)
}

steno_runtime_count_completed_stroke :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	runtime.strokes_since_compaction += 1
	if runtime.strokes_since_compaction < TRANSLATION_COMPACT_INTERVAL_STROKES {
		return
	}

	keep_strokes := TRANSLATION_HISTORY_STROKE_LIMIT
	lookup_strokes := simple_engine_lookup_stroke_limit(runtime.engine.dictionary)
	if keep_strokes < lookup_strokes {
		keep_strokes = lookup_strokes
	}
	simple_engine_compact_history(&runtime.engine, keep_strokes)
	runtime.strokes_since_compaction = 0
}

steno_phrase_lookup_mode_from_runtime_mode :: proc(mode: Steno_Phrase_Mode) -> Phrase_Lookup_Mode {
	switch mode {
	case .Verbs:
		return .Verbs
	case .Nonverbs:
		return .Nonverbs
	case .All, .None:
		return .All
	}
	return .All
}

steno_normalize_stroke_phrase_mode :: proc(stroke: Stroke_Input, current_mode: Steno_Phrase_Mode) -> Steno_Phrase_Mode {
	if stroke.phrase_mode != .None {
		return stroke.phrase_mode
	}
	if stroke.phrase {
		return .All
	}
	return current_mode
}

steno_runtime_trace_label :: proc(mode: Trace_Stroke_Mode) -> string {
	switch mode {
	case .Phrase:
		return " [phrase]"
	case .Phase_Fallback:
		return " [phase fallback]"
	case .Normal:
		return ""
	}
	return ""
}

steno_runtime_write_trace :: proc(runtime: ^Steno_Runtime, outline: string, translation: string, has_translation: bool, mode: Trace_Stroke_Mode) -> bool {
	if runtime.write_trace == nil {
		return true
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, outline)
	formatted_append_string(&buffer, steno_runtime_trace_label(mode))
	formatted_append_string(&buffer, " -> ")
	if has_translation {
		formatted_append_string(&buffer, translation)
	} else {
		formatted_append_string(&buffer, "[untranslated]")
	}
	append(&buffer, '\n')

	line, line_ok := clone_bytes_to_string(buffer[:])
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)
	return runtime.write_trace(line, runtime.userdata)
}

steno_runtime_single_stroke_outline :: proc(bits: u64) -> (outline: string, ok: bool) {
	strokes := [?]u64{bits}
	return stroke_sequence_to_string_alloc(strokes[:])
}

steno_runtime_translate_dictionary_bits :: proc(runtime: ^Steno_Runtime, bits: u64, trace_mode: Trace_Stroke_Mode) -> (ok: bool, maybe_suggest: bool) {
	if bits == 0 {
		return true, false
	}

	raw_chord, raw_ok := steno_runtime_single_stroke_outline(bits)
	if !raw_ok {
		return false, false
	}
	defer owned_string_delete(raw_chord)

	if translation, found := dictionary_lookup_bits(runtime.engine.dictionary, bits); found && len(translation) > 0 && translation[0] == '=' {
		if !steno_runtime_write_trace(runtime, raw_chord, translation, true, trace_mode) {
			return false, false
		}
		return simple_engine_execute_command(&runtime.engine, translation, bits), false
	}

	match, match_ok := simple_engine_find_match(&runtime.engine, bits)
	if !match_ok {
		return false, false
	}
	defer translation_match_destroy(&match)

	trace_translation := match.translation
	trace_has_translation := match.found
	if match.suffix_match {
		trace_has_translation = len(match.suffix_base_translation) > 0 && len(match.suffix_translation) > 0
		trace_translation = ""
	}
	if !steno_runtime_write_trace(runtime, match.outline, trace_translation, trace_has_translation, trace_mode) {
		return false, false
	}
	if !simple_engine_apply_match(&runtime.engine, &match) {
		return false, false
	}
	return true, match.found
}

steno_runtime_translate_phrase_namespace_bits :: proc(runtime: ^Steno_Runtime, bits: u64, phrase_mode: Steno_Phrase_Mode) -> (ok: bool, maybe_suggest: bool) {
	if bits == 0 {
		return true, false
	}
	if runtime.phrasing == nil {
		return false, false
	}

	raw_chord, raw_ok := steno_runtime_single_stroke_outline(bits)
	if !raw_ok {
		return false, false
	}
	defer owned_string_delete(raw_chord)

	text, result := phrasing_lookup_mode(
		runtime.phrasing,
		bits,
		steno_phrase_lookup_mode_from_runtime_mode(phrase_mode),
	)
	defer owned_string_delete(text)

	switch result {
	case .Hit:
		if !steno_runtime_write_trace(runtime, raw_chord, text, true, .Phrase) {
			return false, false
		}
		return simple_engine_apply_single_stroke_translation(&runtime.engine, bits, text), false
	case .Error:
		return false, false
	case .Miss:
	}

	if phrase_namespace_should_fallback_to_dictionary(bits) {
		return steno_runtime_translate_dictionary_bits(runtime, bits, .Phase_Fallback)
	}

	if !steno_runtime_write_trace(runtime, raw_chord, "", false, .Phrase) {
		return false, false
	}
	return simple_engine_apply_single_stroke_translation(&runtime.engine, bits, raw_chord), false
}

steno_runtime_translate_stroke :: proc(runtime: ^Steno_Runtime, stroke: Stroke_Input) -> (ok: bool, maybe_suggest: bool) {
	phrase_namespace := stroke.phrase_namespace || runtime.phrase_namespace_enabled
	if !phrase_namespace {
		return steno_runtime_translate_dictionary_bits(runtime, stroke.bits, .Normal)
	}

	phrase_mode := steno_normalize_stroke_phrase_mode(stroke, runtime.phrase_mode)
	if phrase_mode == .None {
		return steno_runtime_translate_dictionary_bits(runtime, stroke.bits, .Normal)
	}
	return steno_runtime_translate_phrase_namespace_bits(runtime, stroke.bits, phrase_mode)
}

common_utf8_prefix_bytes :: proc(a: string, b: string) -> int {
	index := 0
	last_boundary := 0
	for index < len(a) && index < len(b) && a[index] == b[index] {
		index += 1
		if index >= len(a) || index >= len(b) || (a[index] & 0xC0) != 0x80 {
			last_boundary = index
		}
	}
	return last_boundary
}

steno_runtime_replace_output_text :: proc(runtime: ^Steno_Runtime, old_text: string, new_text: string) -> bool {
	prefix := common_utf8_prefix_bytes(old_text, new_text)
	delete_suffix := old_text[prefix:]
	insert_suffix := new_text[prefix:]

	if len(delete_suffix) > 0 && !runtime.delete_text(delete_suffix, runtime.userdata) {
		return false
	}
	if len(insert_suffix) > 0 && !runtime.send_text(insert_suffix, runtime.userdata) {
		return false
	}
	return true
}

steno_runtime_send_key_combinations :: proc(runtime: ^Steno_Runtime, first_combo: int) -> bool {
	if first_combo >= len(runtime.engine.key_combos) {
		return true
	}
	if runtime.send_key_combination == nil {
		return false
	}
	for i := first_combo; i < len(runtime.engine.key_combos); i += 1 {
		if !runtime.send_key_combination(runtime.engine.key_combos[i], runtime.userdata) {
			return false
		}
	}
	return true
}

steno_runtime_write_suggestion_line :: proc(runtime: ^Steno_Runtime, suggestion: ^Brevity_Suggestion) -> bool {
	if runtime.write_suggestion == nil {
		return true
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, "Suggestion: Use ")
	formatted_append_string(&buffer, suggestion.suggested_outline)
	formatted_append_string(&buffer, " for \"")
	formatted_append_string(&buffer, suggestion.text)
	formatted_append_string(&buffer, "\"\n")

	line, line_ok := clone_bytes_to_string(buffer[:])
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)
	return runtime.write_suggestion(line, runtime.userdata)
}

steno_runtime_write_suggestion_log_line :: proc(runtime: ^Steno_Runtime, suggestion: ^Brevity_Suggestion) -> bool {
	if runtime.write_suggestion_log == nil {
		return true
	}

	line, line_ok := brevity_suggestion_log_line(suggestion, time.time_to_unix(time.now()))
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, line)
	append(&buffer, '\n')
	log_line, log_line_ok := clone_bytes_to_string(buffer[:])
	if !log_line_ok {
		return false
	}
	defer owned_string_delete(log_line)
	return runtime.write_suggestion_log(log_line, runtime.userdata)
}

steno_runtime_maybe_emit_brevity_suggestion :: proc(runtime: ^Steno_Runtime) -> bool {
	if runtime.write_suggestion == nil && runtime.write_suggestion_log == nil {
		return true
	}

	suggestion, found := brevity_suggest(&runtime.engine)
	if !found {
		return true
	}
	defer brevity_suggestion_destroy(&suggestion)

	return steno_runtime_write_suggestion_log_line(runtime, &suggestion) &&
		steno_runtime_write_suggestion_line(runtime, &suggestion)
}

steno_runtime_handle_stroke :: proc(runtime: ^Steno_Runtime, stroke: Stroke_Input) -> bool {
	if runtime == nil || !runtime.session_active {
		return false
	}

	old_text, old_ok := simple_engine_render(&runtime.engine)
	if !old_ok {
		return false
	}
	defer owned_string_delete(old_text)
	first_combo := len(runtime.engine.key_combos)

	translated, maybe_suggest := steno_runtime_translate_stroke(runtime, stroke)
	if !translated {
		return false
	}

	new_text, new_ok := simple_engine_render(&runtime.engine)
	if !new_ok {
		return false
	}
	defer owned_string_delete(new_text)

	if !steno_runtime_replace_output_text(runtime, old_text, new_text) {
		return false
	}
	if !steno_runtime_send_key_combinations(runtime, first_combo) {
		return false
	}
	if maybe_suggest && !steno_runtime_maybe_emit_brevity_suggestion(runtime) {
		return false
	}

	steno_runtime_count_completed_stroke(runtime)
	return true
}

steno_runtime_handle_stroke_bits :: proc(runtime: ^Steno_Runtime, bits: u64) -> bool {
	return steno_runtime_handle_stroke(runtime, Stroke_Input{bits = bits})
}
