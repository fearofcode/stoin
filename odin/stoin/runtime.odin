package stoin

Send_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Delete_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Send_Key_Combination_Callback :: proc(combo: string, userdata: rawptr) -> bool

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
	userdata:             rawptr,
}

Steno_Runtime :: struct {
	engine:                   Simple_Engine,
	phrasing:                 ^Phrasing,
	send_text:                Send_Text_Callback,
	delete_text:              Delete_Text_Callback,
	send_key_combination:     Send_Key_Combination_Callback,
	userdata:                 rawptr,
	session_active:           bool,
	phrase_namespace_enabled: bool,
	phrase_mode:              Steno_Phrase_Mode,
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

steno_runtime_translate_stroke :: proc(runtime: ^Steno_Runtime, stroke: Stroke_Input) -> bool {
	phrase_namespace := stroke.phrase_namespace || runtime.phrase_namespace_enabled
	if !phrase_namespace {
		return simple_engine_translate_bits(&runtime.engine, stroke.bits)
	}

	phrase_mode := steno_normalize_stroke_phrase_mode(stroke, runtime.phrase_mode)
	if phrase_mode == .None {
		return simple_engine_translate_bits(&runtime.engine, stroke.bits)
	}
	if runtime.phrasing == nil {
		return false
	}
	return simple_engine_translate_phrase_bits(
		&runtime.engine,
		runtime.phrasing,
		stroke.bits,
		steno_phrase_lookup_mode_from_runtime_mode(phrase_mode),
	)
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

	if !steno_runtime_translate_stroke(runtime, stroke) {
		return false
	}

	new_text, new_ok := simple_engine_render(&runtime.engine)
	if !new_ok {
		return false
	}
	defer owned_string_delete(new_text)

	return steno_runtime_replace_output_text(runtime, old_text, new_text) &&
		steno_runtime_send_key_combinations(runtime, first_combo)
}

steno_runtime_handle_stroke_bits :: proc(runtime: ^Steno_Runtime, bits: u64) -> bool {
	return steno_runtime_handle_stroke(runtime, Stroke_Input{bits = bits})
}
