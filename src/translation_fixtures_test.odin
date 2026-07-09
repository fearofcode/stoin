package stoin

import "core:os"
import "core:testing"

translation_fixture_next_line :: proc(text: string, cursor: ^int) -> (line: string, ok: bool) {
	if cursor^ >= len(text) {
		return "", false
	}

	start := cursor^
	for cursor^ < len(text) && text[cursor^] != '\n' {
		cursor^ += 1
	}
	line = text[start:cursor^]
	if cursor^ < len(text) && text[cursor^] == '\n' {
		cursor^ += 1
	}
	if len(line) > 0 && line[len(line) - 1] == '\r' {
		line = line[:len(line) - 1]
	}
	return line, true
}

translation_fixture_split :: proc(line: string) -> (name: string, expected: string, outlines: string, events: string, ok: bool) {
	first_tab := -1
	for i in 0..<len(line) {
		if line[i] == '\t' {
			first_tab = i
			break
		}
	}
	if first_tab < 0 {
		return "", "", "", "", false
	}

	second_tab := -1
	for i in first_tab + 1..<len(line) {
		if line[i] == '\t' {
			second_tab = i
			break
		}
	}
	if second_tab < 0 {
		return "", "", "", "", false
	}

	third_tab := -1
	for i in second_tab + 1..<len(line) {
		if line[i] == '\t' {
			third_tab = i
			break
		}
	}
	if third_tab < 0 {
		return "", "", "", "", false
	}

	return line[:first_tab], line[first_tab + 1:second_tab], line[second_tab + 1:third_tab], line[third_tab + 1:], true
}

phrase_translation_fixture_split :: proc(line: string) -> (name: string, mode: string, expected: string, outlines: string, events: string, ok: bool) {
	first_tab := -1
	for i in 0..<len(line) {
		if line[i] == '\t' {
			first_tab = i
			break
		}
	}
	if first_tab < 0 {
		return "", "", "", "", "", false
	}

	second_tab := -1
	for i in first_tab + 1..<len(line) {
		if line[i] == '\t' {
			second_tab = i
			break
		}
	}
	if second_tab < 0 {
		return "", "", "", "", "", false
	}

	third_tab := -1
	for i in second_tab + 1..<len(line) {
		if line[i] == '\t' {
			third_tab = i
			break
		}
	}
	if third_tab < 0 {
		return "", "", "", "", "", false
	}

	fourth_tab := -1
	for i in third_tab + 1..<len(line) {
		if line[i] == '\t' {
			fourth_tab = i
			break
		}
	}
	if fourth_tab < 0 {
		return "", "", "", "", "", false
	}

	return line[:first_tab], line[first_tab + 1:second_tab], line[second_tab + 1:third_tab], line[third_tab + 1:fourth_tab], line[fourth_tab + 1:], true
}

phrase_translation_fixture_mode :: proc(text: string) -> (mode: Steno_Phrase_Mode, ok: bool) {
	switch text {
	case "all":
		return .All, true
	case "verbs":
		return .Verbs, true
	case "nonverbs":
		return .Nonverbs, true
	}
	return .None, false
}

translation_fixture_event_log :: proc(output: ^Runtime_Test_Output) -> (text: string, ok: bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	for event, index in output.events {
		if index > 0 {
			formatted_append_string(&buffer, "|")
		}
		formatted_append_string(&buffer, event)
	}

	return clone_bytes_to_string(buffer[:])
}

translation_fixture_next_outline :: proc(outlines: string, cursor: ^int) -> (outline: string, ok: bool) {
	for cursor^ < len(outlines) && outlines[cursor^] == ' ' {
		cursor^ += 1
	}
	if cursor^ >= len(outlines) {
		return "", false
	}

	start := cursor^
	for cursor^ < len(outlines) && outlines[cursor^] != ' ' {
		cursor^ += 1
	}
	return outlines[start:cursor^], true
}

translation_fixture_run :: proc(t: ^testing.T, dictionary: ^Dictionary, name: string, expected: string, outlines: string, expected_events: string) {
	testing.expect(t, len(name) > 0)

	output: Runtime_Test_Output
	runtime_test_output_init(&output)
	defer runtime_test_output_destroy(&output)

	runtime: Steno_Runtime
	config := runtime_test_config(dictionary, nil, nil, &output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	defer steno_runtime_destroy(&runtime)

	cursor := 0
	stroke_count := 0
	for {
		outline, outline_ok := translation_fixture_next_outline(outlines, &cursor)
		if !outline_ok {
			break
		}
		stroke_count += 1

		bits, parsed := stroke_string_to_bits(outline)
		testing.expect(t, parsed)
		if !parsed {
			return
		}
		testing.expect(t, steno_runtime_handle_stroke_bits(&runtime, bits))
	}
	testing.expect(t, stroke_count > 0)

	testing.expect_value(t, string(output.text[:]), expected)

	events, events_ok := translation_fixture_event_log(&output)
	defer owned_string_delete(events)
	testing.expect(t, events_ok)
	testing.expect_value(t, events, expected_events)
}

phrase_translation_fixture_run :: proc(
	t: ^testing.T,
	dictionary: ^Dictionary,
	phrasing: ^Phrasing,
	name: string,
	mode: Steno_Phrase_Mode,
	expected: string,
	outlines: string,
	expected_events: string,
) {
	testing.expect(t, len(name) > 0)

	output: Runtime_Test_Output
	runtime_test_output_init(&output)
	defer runtime_test_output_destroy(&output)

	runtime: Steno_Runtime
	config := runtime_test_config(dictionary, phrasing, nil, &output)
	testing.expect(t, steno_runtime_init(&runtime, &config))
	defer steno_runtime_destroy(&runtime)

	cursor := 0
	stroke_count := 0
	for {
		outline, outline_ok := translation_fixture_next_outline(outlines, &cursor)
		if !outline_ok {
			break
		}
		stroke_count += 1

		bits, parsed := stroke_string_to_bits(outline)
		testing.expect(t, parsed)
		if !parsed {
			return
		}
		testing.expect(t, steno_runtime_handle_stroke(&runtime, Stroke_Input {
			bits = bits,
			phrase_namespace = true,
			phrase_mode = mode,
		}))
	}
	testing.expect(t, stroke_count > 0)

	testing.expect_value(t, string(output.text[:]), expected)

	events, events_ok := translation_fixture_event_log(&output)
	defer owned_string_delete(events)
	testing.expect(t, events_ok)
	testing.expect_value(t, events, expected_events)
}

@(test)
test_shared_translation_fixtures :: proc(t: ^testing.T) {
	data, read_err := os.read_entire_file("tests/translation-fixtures.tsv", context.allocator)
	testing.expect(t, read_err == nil)
	if read_err != nil {
		return
	}
	defer delete(data)

	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	text := string(data)
	cursor := 0
	fixture_count := 0

	for {
		line, line_ok := translation_fixture_next_line(text, &cursor)
		if !line_ok {
			break
		}
		if len(line) == 0 || line[0] == '#' {
			continue
		}

		name, expected, outlines, events, split_ok := translation_fixture_split(line)
		testing.expect(t, split_ok)
		if !split_ok {
			continue
		}

		fixture_count += 1
		translation_fixture_run(t, &dictionary, name, expected, outlines, events)
	}

	testing.expect(t, fixture_count > 0)
}

@(test)
test_shared_phrase_translation_fixtures :: proc(t: ^testing.T) {
	data, read_err := os.read_entire_file("tests/phrase-translation-fixtures.tsv", context.allocator)
	testing.expect(t, read_err == nil)
	if read_err != nil {
		return
	}
	defer delete(data)

	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	phrasing, phrasing_ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, phrasing_ok)

	text := string(data)
	cursor := 0
	fixture_count := 0

	for {
		line, line_ok := translation_fixture_next_line(text, &cursor)
		if !line_ok {
			break
		}
		if len(line) == 0 || line[0] == '#' {
			continue
		}

		name, mode_text, expected, outlines, events, split_ok := phrase_translation_fixture_split(line)
		testing.expect(t, split_ok)
		if !split_ok {
			continue
		}
		mode, mode_ok := phrase_translation_fixture_mode(mode_text)
		testing.expect(t, mode_ok)
		if !mode_ok {
			continue
		}

		fixture_count += 1
		phrase_translation_fixture_run(t, &dictionary, &phrasing, name, mode, expected, outlines, events)
	}

	testing.expect(t, fixture_count > 0)
}
