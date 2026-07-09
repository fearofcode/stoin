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

translation_fixture_split :: proc(line: string) -> (name: string, expected: string, outlines: string, ok: bool) {
	first_tab := -1
	for i in 0..<len(line) {
		if line[i] == '\t' {
			first_tab = i
			break
		}
	}
	if first_tab < 0 {
		return "", "", "", false
	}

	second_tab := -1
	for i in first_tab + 1..<len(line) {
		if line[i] == '\t' {
			second_tab = i
			break
		}
	}
	if second_tab < 0 {
		return "", "", "", false
	}

	return line[:first_tab], line[first_tab + 1:second_tab], line[second_tab + 1:], true
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

translation_fixture_run :: proc(t: ^testing.T, dictionary: ^Dictionary, name: string, expected: string, outlines: string) {
	testing.expect(t, len(name) > 0)

	engine: Simple_Engine
	simple_engine_init(&engine, dictionary)
	defer simple_engine_destroy(&engine)

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
		testing.expect(t, simple_engine_translate_bits(&engine, bits))
	}
	testing.expect(t, stroke_count > 0)

	text, render_ok := simple_engine_render(&engine)
	defer owned_string_delete(text)
	testing.expect(t, render_ok)
	testing.expect_value(t, text, expected)
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

		name, expected, outlines, split_ok := translation_fixture_split(line)
		testing.expect(t, split_ok)
		if !split_ok {
			continue
		}

		fixture_count += 1
		translation_fixture_run(t, &dictionary, name, expected, outlines)
	}

	testing.expect(t, fixture_count > 0)
}
