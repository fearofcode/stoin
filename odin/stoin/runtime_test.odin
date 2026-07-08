package stoin

import "core:testing"

Runtime_Test_Output :: struct {
	text:       [dynamic]byte,
	sends:      [dynamic]string,
	deletes:    [dynamic]string,
	key_combos: [dynamic]string,
}

runtime_test_output_init :: proc(output: ^Runtime_Test_Output) {
	output^ = {}
	output.text = make([dynamic]byte)
	output.sends = make([dynamic]string)
	output.deletes = make([dynamic]string)
	output.key_combos = make([dynamic]string)
}

runtime_test_output_destroy_string_list :: proc(values: ^[dynamic]string) {
	for value in values^ {
		owned_string_delete(value)
	}
	delete(values^)
	values^ = nil
}

runtime_test_output_destroy :: proc(output: ^Runtime_Test_Output) {
	delete(output.text)
	runtime_test_output_destroy_string_list(&output.sends)
	runtime_test_output_destroy_string_list(&output.deletes)
	runtime_test_output_destroy_string_list(&output.key_combos)
	output^ = {}
}

runtime_test_output_reset_events :: proc(output: ^Runtime_Test_Output) {
	for value in output.sends {
		owned_string_delete(value)
	}
	clear(&output.sends)
	for value in output.deletes {
		owned_string_delete(value)
	}
	clear(&output.deletes)
	for value in output.key_combos {
		owned_string_delete(value)
	}
	clear(&output.key_combos)
}

runtime_test_output_clear :: proc(output: ^Runtime_Test_Output) {
	clear(&output.text)
	runtime_test_output_reset_events(output)
}

runtime_test_record :: proc(values: ^[dynamic]string, text: string) -> bool {
	copy, ok := clone_string_ok(text)
	if !ok {
		return false
	}
	append(values, copy)
	return true
}

runtime_test_send_text :: proc(text: string, userdata: rawptr) -> bool {
	output := (^Runtime_Test_Output)(userdata)
	if output == nil {
		return false
	}
	if !runtime_test_record(&output.sends, text) {
		return false
	}
	for b in transmute([]byte)text {
		append(&output.text, b)
	}
	return true
}

runtime_test_delete_text :: proc(text: string, userdata: rawptr) -> bool {
	output := (^Runtime_Test_Output)(userdata)
	if output == nil {
		return false
	}
	if len(text) == 0 {
		return true
	}
	if len(text) > len(output.text) {
		return false
	}

	start := len(output.text) - len(text)
	if string(output.text[start:]) != text {
		return false
	}
	if !runtime_test_record(&output.deletes, text) {
		return false
	}
	resize(&output.text, start)
	return true
}

runtime_test_send_key_combination :: proc(combo: string, userdata: rawptr) -> bool {
	output := (^Runtime_Test_Output)(userdata)
	if output == nil {
		return false
	}
	return runtime_test_record(&output.key_combos, combo)
}

runtime_test_last :: proc(values: []string) -> string {
	if len(values) == 0 {
		return ""
	}
	return values[len(values) - 1]
}

runtime_test_bits :: proc(t: ^testing.T, outline: string) -> u64 {
	bits, ok := stroke_string_to_bits(outline)
	testing.expect(t, ok)
	return bits
}

runtime_test_expect_output :: proc(t: ^testing.T, output: ^Runtime_Test_Output, expected: string) {
	testing.expect_value(t, string(output.text[:]), expected)
}

runtime_test_config :: proc(dictionary: ^Dictionary, phrasing: ^Phrasing, orthography: ^Orthography, output: ^Runtime_Test_Output) -> Steno_Runtime_Config {
	return Steno_Runtime_Config {
		dictionary = dictionary,
		orthography = orthography,
		phrasing = phrasing,
		send_text = runtime_test_send_text,
		delete_text = runtime_test_delete_text,
		send_key_combination = runtime_test_send_key_combination,
		userdata = rawptr(output),
	}
}

@(test)
test_steno_runtime_emits_minimal_text_replacements :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	output: Runtime_Test_Output
	runtime_test_output_init(&output)
	defer runtime_test_output_destroy(&output)

	runtime: Steno_Runtime
	config := runtime_test_config(&dictionary, nil, nil, &output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	defer steno_runtime_destroy(&runtime)

	testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, runtime_test_bits(t, "STOER")))
	runtime_test_expect_output(t, &output, "story")
	testing.expect_value(t, len(output.sends), 1)
	testing.expect_value(t, len(output.deletes), 0)
	testing.expect_value(t, runtime_test_last(output.sends[:]), "story")

	runtime_test_output_reset_events(&output)
	testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, runtime_test_bits(t, "-Z")))
	runtime_test_expect_output(t, &output, "stories")
	testing.expect_value(t, len(output.sends), 1)
	testing.expect_value(t, len(output.deletes), 1)
	testing.expect_value(t, runtime_test_last(output.deletes[:]), "y")
	testing.expect_value(t, runtime_test_last(output.sends[:]), "ies")

	runtime_test_output_reset_events(&output)
	testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, runtime_test_bits(t, "-R")))
	runtime_test_expect_output(t, &output, "story")
	testing.expect_value(t, len(output.sends), 1)
	testing.expect_value(t, len(output.deletes), 1)
	testing.expect_value(t, runtime_test_last(output.deletes[:]), "ies")
	testing.expect_value(t, runtime_test_last(output.sends[:]), "y")
}

@(test)
test_steno_runtime_phrase_namespace_gate :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	phrasing, phrasing_ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, phrasing_ok)

	output: Runtime_Test_Output
	runtime_test_output_init(&output)
	defer runtime_test_output_destroy(&output)

	runtime: Steno_Runtime
	config := runtime_test_config(&dictionary, &phrasing, nil, &output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	defer steno_runtime_destroy(&runtime)

	is_a_bits := runtime_test_bits(t, "PW-B")
	testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, is_a_bits))
	runtime_test_expect_output(t, &output, "dictionary is a")

	steno_runtime_destroy(&runtime)
	runtime_test_output_clear(&output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	steno_runtime_set_phrase_namespace_enabled(&runtime, true)
	testing.expect(t, steno_runtime_handle_stroke(&runtime, Stroke_Input{bits = is_a_bits}))
	runtime_test_expect_output(t, &output, "dictionary is a")

	steno_runtime_destroy(&runtime)
	runtime_test_output_clear(&output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	testing.expect(t, steno_runtime_handle_stroke(&runtime, Stroke_Input {
		bits = is_a_bits,
		phrase_namespace = true,
		phrase_mode = .Verbs,
	}))
	runtime_test_expect_output(t, &output, "is a")
}

@(test)
test_steno_runtime_sends_key_combinations :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	output: Runtime_Test_Output
	runtime_test_output_init(&output)
	defer runtime_test_output_destroy(&output)

	runtime: Steno_Runtime
	config := runtime_test_config(&dictionary, nil, nil, &output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	defer steno_runtime_destroy(&runtime)

	testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, runtime_test_bits(t, "STPH-G")))
	runtime_test_expect_output(t, &output, "")
	testing.expect_value(t, len(output.key_combos), 1)
	testing.expect_value(t, runtime_test_last(output.key_combos[:]), "Right")
}
