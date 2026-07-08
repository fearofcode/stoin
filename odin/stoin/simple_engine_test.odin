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

test_suggestion_after_sequence :: proc(t: ^testing.T, dictionary: ^Dictionary, outlines: []string, expected_outline: string, expected_text: string, expected_typed_outline: string, expected_typed_strokes: int) {
	engine: Simple_Engine
	simple_engine_init(&engine, dictionary)
	defer simple_engine_destroy(&engine)

	for outline in outlines {
		bits, parsed := stroke_string_to_bits(outline)
		testing.expect(t, parsed)
		testing.expect(t, simple_engine_translate_bits(&engine, bits))
	}

	suggestion, found := brevity_suggest(&engine)
	defer brevity_suggestion_destroy(&suggestion)
	testing.expect(t, found)
	testing.expect_value(t, suggestion.suggested_outline, expected_outline)
	testing.expect_value(t, suggestion.text, expected_text)
	testing.expect_value(t, suggestion.typed_outline, expected_typed_outline)
	testing.expect_value(t, suggestion.typed_strokes, expected_typed_strokes)
	testing.expect(t, suggestion.saved_strokes > 0)
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
test_format_translation_tracks_first_orthographic_suffix :: proc(t: ^testing.T) {
	formatted, ok := format_translation_text_basic("{^er}{^s}")
	defer formatted_text_destroy(&formatted)

	testing.expect(t, ok)
	testing.expect_value(t, formatted.text, "ers")
	testing.expect_value(t, formatted.ortho_suffix, "er")
	testing.expect_value(t, formatted.ortho_suffix_text_offset, 0)
	testing.expect_value(t, formatted.ortho_suffix_text_length, 2)
}

@(test)
test_format_translation_stitch_metadata :: proc(t: ^testing.T) {
	formatted, ok := format_translation_text_basic("{:stitch:A}")
	defer formatted_text_destroy(&formatted)

	testing.expect(t, ok)
	testing.expect_value(t, formatted.text, "A")
	testing.expect(t, formatted.stitch)
	testing.expect(t, formatted.glue)
	testing.expect_value(t, formatted_stitch_delimiter(&formatted), "-")

	retro, retro_ok := format_translation_text_basic("{:stitch_last_word:2:-}")
	defer formatted_text_destroy(&retro)
	testing.expect(t, retro_ok)
	testing.expect(t, retro.stitch_last_word)
	testing.expect_value(t, retro.stitch_count, 2)
	testing.expect_value(t, formatted_stitch_delimiter(&retro), "-")
}

@(test)
test_format_translation_retro_commands :: proc(t: ^testing.T) {
	toggle, toggle_ok := format_translation_text_basic("{*}")
	defer formatted_text_destroy(&toggle)
	testing.expect(t, toggle_ok)
	testing.expect_value(t, toggle.retro_command, Retro_Command.Toggle_Asterisk)
	testing.expect_value(t, toggle.text, "")

	delete_space, delete_ok := format_translation_text_basic("{*!}")
	defer formatted_text_destroy(&delete_space)
	testing.expect(t, delete_ok)
	testing.expect_value(t, delete_space.retro_command, Retro_Command.Delete_Space)

	insert_space, insert_ok := format_translation_text_basic("{*?}")
	defer formatted_text_destroy(&insert_space)
	testing.expect(t, insert_ok)
	testing.expect_value(t, insert_space.retro_command, Retro_Command.Insert_Space)
}

@(test)
test_format_translation_case_metadata :: proc(t: ^testing.T) {
	case_next, case_ok := format_translation_text_basic("{}{:case:cap_first_word}")
	defer formatted_text_destroy(&case_next)
	testing.expect(t, case_ok)
	testing.expect_value(t, case_next.text, "")
	testing.expect_value(t, case_next.next_case, Case_Mode.Cap_First_Word)

	period, period_ok := format_translation_text_basic("{.}")
	defer formatted_text_destroy(&period)
	testing.expect(t, period_ok)
	testing.expect_value(t, period.next_case, Case_Mode.Cap_First_Word)

	retro, retro_ok := format_translation_text_basic("{:retro_case:lower_first_char}")
	defer formatted_text_destroy(&retro)
	testing.expect(t, retro_ok)
	testing.expect_value(t, retro.retro_case, Case_Mode.Lower_First_Char)
}

@(test)
test_format_translation_mode_metadata :: proc(t: ^testing.T) {
	mode, ok := format_translation_text_basic("{MODE:SET_SPACE:}")
	defer formatted_text_destroy(&mode)

	testing.expect(t, ok)
	testing.expect_value(t, mode.text, "")
	testing.expect_value(t, mode.mode_command, "SET_SPACE:")
}

@(test)
test_build_text_does_not_alias_old_text :: proc(t: ^testing.T) {
	old_text, old_ok := clone_string_ok("quick")
	testing.expect(t, old_ok)

	engine: Simple_Engine
	simple_engine_init(&engine, nil)
	defer simple_engine_destroy(&engine)

	formatted := Formatted_Text{text = "ly", attach_prev = true}
	next_text, next_ok := simple_engine_build_text(&engine, old_text, nil, &formatted)
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
test_simple_engine_suffix_key_matches :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	cherries := [?]string{"KHERZ"}
	test_translate_sequence(t, &dictionary, cherries[:], "cherries")

	deferred := [?]string{"TKEFRD"}
	test_translate_sequence(t, &dictionary, deferred[:], "deferred")

	failing := [?]string{"TPAEULG"}
	test_translate_sequence(t, &dictionary, failing[:], "failing")

	stymied := [?]string{"STAOEU", "PHAOED"}
	test_translate_sequence(t, &dictionary, stymied[:], "stymied")

	history_saps := [?]string{"HEU", "SAPS"}
	test_translate_sequence(t, &dictionary, history_saps[:], "history saps")

	sappers := [?]string{"SAP", "*ERZ"}
	test_translate_sequence(t, &dictionary, sappers[:], "sappers")

	nonfinal_d := [?]string{"WADZ"}
	test_translate_sequence(t, &dictionary, nonfinal_d[:], "WADZ")

	nonfinal_s := [?]string{"KASD"}
	test_translate_sequence(t, &dictionary, nonfinal_s[:], "KASD")

	nonfinal_g := [?]string{"KAURBGS"}
	test_translate_sequence(t, &dictionary, nonfinal_g[:], "KAURBGS")
}

@(test)
test_simple_engine_commands :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	undo_one := [?]string{"F", "-P", "-R"}
	test_translate_sequence(t, &dictionary, undo_one[:], "fee")

	undo_empty := [?]string{"F", "-R", "-R"}
	test_translate_sequence(t, &dictionary, undo_empty[:], "")

	undo_retroactive_match := [?]string{"STOER", "-Z", "-R"}
	test_translate_sequence(t, &dictionary, undo_retroactive_match[:], "story")

	translate_after_retroactive_undo := [?]string{"STOER", "-Z", "-R", "-D"}
	test_translate_sequence(t, &dictionary, translate_after_retroactive_undo[:], "storied")

	repeat_last := [?]string{"KAT", "SKWR"}
	test_translate_sequence(t, &dictionary, repeat_last[:], "cat cat")

	undo_repeat := [?]string{"KAT", "SKWR", "-R"}
	test_translate_sequence(t, &dictionary, undo_repeat[:], "cat")
}

@(test)
test_simple_engine_stitch :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	stitch_letters := [?]string{"A", "PW", "KR"}
	test_translate_sequence(t, &dictionary, stitch_letters[:], "A-B-C")

	stitch_last_word := [?]string{"TEFT", "-RBGS"}
	test_translate_sequence(t, &dictionary, stitch_last_word[:], "t-e-s-t")

	stitch_last_word_after_previous := [?]string{"AOEU", "TO", "-RBGS"}
	test_translate_sequence(t, &dictionary, stitch_last_word_after_previous[:], "eye t-o")

	stitch_last_word_supersedes := [?]string{"AOEU", "TO", "-RBGS", "-RBGS"}
	test_translate_sequence(t, &dictionary, stitch_last_word_supersedes[:], "e-y-e t-o")
}

@(test)
test_simple_engine_retro_commands :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	toggle_star := [?]string{"KAT", "#*"}
	test_translate_sequence(t, &dictionary, toggle_star[:], "kitty")

	delete_space := [?]string{"PWA", "PWAL", "SP-LS"}
	test_translate_sequence(t, &dictionary, delete_space[:], "basketball")

	insert_space := [?]string{"PWA", "PWAL", "SP-LS", "S-PD"}
	test_translate_sequence(t, &dictionary, insert_space[:], "basket ball")
}

@(test)
test_simple_engine_case_formatting :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	period_capitalizes := [?]string{"KAT", "TP-PL", "KAT"}
	test_translate_sequence(t, &dictionary, period_capitalizes[:], "cat. Cat")

	period_space_capitalizes := [?]string{"KAT", "PH-FP", "KAT"}
	test_translate_sequence(t, &dictionary, period_space_capitalizes[:], "cat. Cat")

	comma_does_not_capitalize := [?]string{"KAT", "KW-BG", "KAT"}
	test_translate_sequence(t, &dictionary, comma_does_not_capitalize[:], "cat, cat")

	cap_next := [?]string{"KPA", "KAT"}
	test_translate_sequence(t, &dictionary, cap_next[:], "Cat")

	upper_next := [?]string{"KPA*L", "KAT"}
	test_translate_sequence(t, &dictionary, upper_next[:], "CAT")

	lower_next := [?]string{"HRO*ER", "PHROF"}
	test_translate_sequence(t, &dictionary, lower_next[:], "plover")

	retro_lower_previous := [?]string{"PHROF", "HRO*ERD"}
	test_translate_sequence(t, &dictionary, retro_lower_previous[:], "plover")

	undo_restores_pending_case := [?]string{"KAT", "TP-PL", "KAT", "-R", "KAT"}
	test_translate_sequence(t, &dictionary, undo_restores_pending_case[:], "cat. Cat")
}

@(test)
test_simple_engine_mode_commands :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	caps_word := [?]string{"KA*PS", "KAT"}
	test_translate_sequence(t, &dictionary, caps_word[:], "CAT")

	caps_preserves_spacing := [?]string{"KAT", "KA*PS", "PWAL"}
	test_translate_sequence(t, &dictionary, caps_preserves_spacing[:], "cat BALL")

	reset_mode := [?]string{"R*EFT", "KAT"}
	test_translate_sequence(t, &dictionary, reset_mode[:], "cat")

	lower_mode := [?]string{"WRE", "PHROF"}
	test_translate_sequence(t, &dictionary, lower_mode[:], "plover")

	title_mode := [?]string{"WRU", "KAT"}
	test_translate_sequence(t, &dictionary, title_mode[:], "Cat")

	snake_mode := [?]string{"R*EFT", "WRA", "KAT", "PWAL"}
	test_translate_sequence(t, &dictionary, snake_mode[:], "cat_ball")

	empty_space := [?]string{"R*EFT", "TPHA", "KAT", "PWAL", "KPAO", "KAT"}
	test_translate_sequence(t, &dictionary, empty_space[:], "catball cat")

	camel_mode := [?]string{"R*EFT", "WRO", "KAT", "PWAL"}
	test_translate_sequence(t, &dictionary, camel_mode[:], "catBall")
}

@(test)
test_simple_engine_brevity_suggestions :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	in_the := [?]string{"TPH", "-T"}
	test_suggestion_after_sequence(t, &dictionary, in_the[:], "TPH-T", "in the", "TPH/-T", 2)

	in_the_beginning := [?]string{"TPH", "-T", "PW-G"}
	test_suggestion_after_sequence(t, &dictionary, in_the_beginning[:], "TPH-T/PWG", "in the beginning", "TPH/-T/PWG", 3)

	quickly := [?]string{"KWEUBG", "-L"}
	test_suggestion_after_sequence(t, &dictionary, quickly[:], "KWEUL", "quickly", "KWEUBG/L", 2)

	engine: Simple_Engine
	simple_engine_init(&engine, &dictionary)
	defer simple_engine_destroy(&engine)
	bits, parsed := stroke_string_to_bits("TPH-T")
	testing.expect(t, parsed)
	testing.expect(t, simple_engine_translate_bits(&engine, bits))
	suggestion, found := brevity_suggest(&engine)
	defer brevity_suggestion_destroy(&suggestion)
	testing.expect(t, !found)
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
