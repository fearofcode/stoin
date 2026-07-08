package stoin

import "core:testing"

test_translate_sequence :: proc(t: ^testing.T, dictionary: ^Dictionary, outlines: []string, expected: string) {
	text, ok := translate_outline_sequence(dictionary, outlines)
	defer owned_string_delete(text)

	scratch := make([dynamic]byte)
	defer delete(scratch)
	for _ in 0..<4096 {
		append(&scratch, 0xAA)
	}

	testing.expect(t, ok)
	testing.expect_value(t, text, expected)
}

@(test)
test_basic_format_translation_text :: proc(t: ^testing.T) {
	formatted, ok := format_translation_text_basic("{^ly}")
	defer formatted_text_destroy(&formatted)

	testing.expect(t, ok)
	testing.expect_value(t, formatted.text, "ly")
	testing.expect_value(t, formatted.ortho_suffix, "ly")
	testing.expect(t, formatted.attach_prev)
	testing.expect(t, !formatted.attach_next)
}

@(test)
test_build_text_does_not_alias_old_text :: proc(t: ^testing.T) {
	old_text, old_ok := clone_string_ok("quick")
	testing.expect(t, old_ok)

	formatted := Formatted_Text{text = "ly", attach_prev = true}
	next_text, next_ok := simple_engine_build_text(old_text, nil, &formatted)
	defer owned_string_delete(next_text)
	owned_string_delete(old_text)

	scratch := make([dynamic]byte)
	defer delete(scratch)
	for _ in 0..<4096 {
		append(&scratch, 0xAA)
	}

	testing.expect(t, next_ok)
	testing.expect_value(t, next_text, "quickly")
}

@(test)
test_render_does_not_alias_history_text :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)

	engine: Simple_Engine
	simple_engine_init(&engine, &dictionary)
	text, text_ok := clone_string_ok("quickly")
	testing.expect(t, text_ok)
	append(&engine.history, Applied_Translation{text = text})

	rendered, render_ok := simple_engine_render(&engine)
	defer owned_string_delete(rendered)
	simple_engine_destroy(&engine)

	scratch := make([dynamic]byte)
	defer delete(scratch)
	for _ in 0..<4096 {
		append(&scratch, 0xAA)
	}

	testing.expect(t, render_ok)
	testing.expect_value(t, rendered, "quickly")
}

@(test)
test_simple_engine_translation :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	quickly := [?]string{"KWEUBG", "-L"}
	test_translate_sequence(t, &dictionary, quickly[:], "quickly")

	in_the_beginning := [?]string{"TPH-T", "PW-G"}
	test_translate_sequence(t, &dictionary, in_the_beginning[:], "in the beginning")

	punctuation := [?]string{"KAT", "KW-BG", "KAT"}
	test_translate_sequence(t, &dictionary, punctuation[:], "cat, cat")

	cherries := [?]string{"KHER", "-Z"}
	test_translate_sequence(t, &dictionary, cherries[:], "cherries")

	reddish := [?]string{"RED", "EURB"}
	test_translate_sequence(t, &dictionary, reddish[:], "reddish")
}

@(test)
test_orthography_basic_rules :: proc(t: ^testing.T) {
	cases := [?]struct{word: string, suffix: string, expected: string} {
		{word = "artistic", suffix = "ly", expected = "artistically"},
		{word = "speech", suffix = "s", expected = "speeches"},
		{word = "beach", suffix = "s", expected = "beaches"},
		{word = "stomach", suffix = "s", expected = "stomachs"},
		{word = "monarch", suffix = "s", expected = "monarchs"},
		{word = "cherry", suffix = "s", expected = "cherries"},
		{word = "day", suffix = "s", expected = "days"},
		{word = "pharmacy", suffix = "ist", expected = "pharmacist"},
		{word = "similar", suffix = "ish", expected = "similarish"},
		{word = "red", suffix = "ish", expected = "reddish"},
		{word = "stymie", suffix = "ed", expected = "stymied"},
		{word = "tie", suffix = "ed", expected = "tied"},
	}

	for c in cases {
		actual, ok := orthography_apply_basic(c.word, c.suffix)
		testing.expect(t, ok)
		testing.expect_value(t, actual, c.expected)
		owned_string_delete(actual)
	}
}
