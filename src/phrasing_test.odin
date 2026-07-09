package stoin

import "core:testing"

@(test)
test_phrasing_load_fixture :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)

	testing.expect(t, ok)
	testing.expect_value(t, len(phrasing.iv_tails), 4)
	testing.expect_value(t, len(phrasing.iv_stems), 10)
	testing.expect_value(t, len(phrasing.nv_tails), 6)
	testing.expect_value(t, len(phrasing.nv_prefixes), 5)
	testing.expect_value(t, len(phrasing.fv_starters), 5)
	testing.expect_value(t, len(phrasing.fv_operators), 4)
	testing.expect_value(t, len(phrasing.fv_structures), 3)
	testing.expect_value(t, len(phrasing.fv_verbs), 11)
	testing.expect_value(t, len(phrasing.fv_enders), 27)

	khr_bits, khr_ok := stroke_string_to_bits("KHR")
	testing.expect(t, khr_ok)
	found_call := false
	for stem in phrasing.iv_stems {
		if stem.bits == khr_bits {
			found_call = true
			testing.expect_value(t, len(stem.forms), 7)
		}
	}
	testing.expect(t, found_call)

	testing.expect_value(t, phrasing_find_tail(phrasing.nv_tails[:], "else") >= 0, true)
	say_index := phrasing_find_verb(phrasing.fv_verbs[:], "say")
	testing.expect(t, say_index >= 0)

	says_bits, says_ok := stroke_string_to_bits("-BS")
	testing.expect(t, says_ok)
	found_say_ender := false
	for ender in phrasing.fv_enders {
		if ender.bits == says_bits {
			found_say_ender = true
			testing.expect_value(t, ender.verb_index, say_index)
			testing.expect(t, !ender.past)
		}
	}
	testing.expect(t, found_say_ender)
}

@(test)
test_phrasing_rejects_duplicate_tail_strokes :: proc(t: ^testing.T) {
	raw: Raw_Initial_Verbs
	raw.tails = make([dynamic]Raw_Phrase_Tail)
	raw.stems = make([dynamic]Raw_Iv_Stem)
	defer delete(raw.tails)
	defer delete(raw.stems)

	append(&raw.tails, Raw_Phrase_Tail{id = "a", stroke = "-B", text = "a"})
	append(&raw.tails, Raw_Phrase_Tail{id = "b", stroke = "-B", text = "b"})

	phrasing: Phrasing
	ok := phrasing_parse_initial_verbs(&phrasing, &raw)
	defer phrasing_destroy(&phrasing)
	testing.expect(t, !ok)
}

test_expect_phrase_lookup :: proc(t: ^testing.T, phrasing: ^Phrasing, outline: string, mode: Phrase_Lookup_Mode, expected: string) {
	bits, parsed := stroke_string_to_bits(outline)
	testing.expect(t, parsed)
	text, result := phrasing_lookup_mode(phrasing, bits, mode)
	defer owned_string_delete(text)
	testing.expect_value(t, result, Phrase_Lookup_Result.Hit)
	testing.expect_value(t, text, expected)
}

test_expect_phrase_miss :: proc(t: ^testing.T, phrasing: ^Phrasing, outline: string, mode: Phrase_Lookup_Mode) {
	bits, parsed := stroke_string_to_bits(outline)
	testing.expect(t, parsed)
	text, result := phrasing_lookup_mode(phrasing, bits, mode)
	defer owned_string_delete(text)
	testing.expect_value(t, result, Phrase_Lookup_Result.Miss)
}

@(test)
test_phrasing_lookup_initial_verbs :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, ok)

	test_expect_phrase_lookup(t, &phrasing, "PW-B", .Verbs, "is a")
	test_expect_phrase_lookup(t, &phrasing, "PW-BD", .Verbs, "was a")
	test_expect_phrase_lookup(t, &phrasing, "PWE-BD", .Verbs, "were a")
	test_expect_phrase_lookup(t, &phrasing, "PWU-B", .Verbs, "to be a")
	test_expect_phrase_lookup(t, &phrasing, "THRA-S", .Verbs, "can tell us")
	test_expect_phrase_lookup(t, &phrasing, "KPA-P", .Verbs, "can keep it")
	test_expect_phrase_lookup(t, &phrasing, "KHR-PG", .Verbs, "calling it")
}

@(test)
test_phrasing_lookup_nonverbs :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, ok)

	test_expect_phrase_lookup(t, &phrasing, "TW-B", .Nonverbs, "with a")
	test_expect_phrase_lookup(t, &phrasing, "TW-S", .Nonverbs, "with us")
	test_expect_phrase_lookup(t, &phrasing, "TKPWH*-RT", .Nonverbs, "anything that")
	test_expect_phrase_lookup(t, &phrasing, "TKPWH*-LS", .Nonverbs, "anything else")
	test_expect_phrase_lookup(t, &phrasing, "S*-F", .Nonverbs, "as if")
	test_expect_phrase_lookup(t, &phrasing, "S*-GT", .Nonverbs, "as though")
	test_expect_phrase_lookup(t, &phrasing, "SRAO*E-S", .Nonverbs, "even us")
	test_expect_phrase_lookup(t, &phrasing, "SRAO*E-RT", .Nonverbs, "even that")
	test_expect_phrase_lookup(t, &phrasing, "SRAO*E-GT", .Nonverbs, "even though")
}

@(test)
test_phrasing_lookup_namespaces :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, ok)

	test_expect_phrase_lookup(t, &phrasing, "PW-B", .Verbs, "is a")
	test_expect_phrase_lookup(t, &phrasing, "PW-B", .Nonverbs, "near a")
	test_expect_phrase_lookup(t, &phrasing, "PW-B", .All, "is a")
	test_expect_phrase_miss(t, &phrasing, "TW-B", .Verbs)
}

@(test)
test_phrasing_lookup_final_verbs_long :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, ok)

	cases := [?]struct{outline: string, expected: string} {
		{outline = "SKWHR-B", expected = "she is"},
		{outline = "SKWHR-BD", expected = "she was"},
		{outline = "SKWHR*E", expected = "she is not"},
		{outline = "SKWHR*ED", expected = "she was not"},
		{outline = "SWR-F", expected = "I have"},
		{outline = "SWR-FD", expected = "I had"},
		{outline = "KPWR-GD", expected = "you went"},
		{outline = "SKWHRAO-G", expected = "she will go"},
		{outline = "SKWHRAO*G", expected = "she will not go"},
		{outline = "SKWHREG", expected = "she is going"},
		{outline = "SKWHR-FG", expected = "she has gone"},
		{outline = "KPWR-PBT", expected = "you know that"},
		{outline = "SKWHR-PBG", expected = "she thinks"},
		{outline = "SKWHR-PBGD", expected = "she thought"},
		{outline = "SKWHR-PBGT", expected = "she thinks that"},
		{outline = "SKWHR-PBGTD", expected = "she thought that"},
		{outline = "SKWHR-BS", expected = "she says"},
		{outline = "SKWHR-BSD", expected = "she said"},
		{outline = "SKWHR-BTS", expected = "she says that"},
		{outline = "SKWHR-BTSD", expected = "she said that"},
		{outline = "SKWHR-RLT", expected = "she tells"},
		{outline = "SKWHR-RLTD", expected = "she told"},
		{outline = "SKWHR-FPL", expected = "she holds"},
		{outline = "SKWHR-FPLD", expected = "she held"},
		{outline = "SKWHR-LS", expected = "she sells"},
		{outline = "SKWHR-LSD", expected = "she sold"},
		{outline = "SKWHR-PLS", expected = "she spells"},
		{outline = "SKWHR-PLSD", expected = "she spelled"},
		{outline = "SKWHR-RPBTS", expected = "she keeps"},
		{outline = "SKWHR-RPBTSD", expected = "she kept"},
		{outline = "TWH-TS", expected = "they have to"},
	}

	for c in cases {
		test_expect_phrase_lookup(t, &phrasing, c.outline, .Verbs, c.expected)
	}
}

@(test)
test_phrasing_lookup_final_verb_contractions :: proc(t: ^testing.T) {
	phrasing, ok := phrasing_load("tests/test-phrasing.json")
	defer phrasing_destroy(&phrasing)
	testing.expect(t, ok)

	cases := [?]struct{outline: string, expected: string} {
		{outline = "#SKWHR-B", expected = "she's"},
		{outline = "#SKWHR*E", expected = "she isn't"},
		{outline = "#SKWHR*ED", expected = "she wasn't"},
		{outline = "#SKWHRAO-G", expected = "she'll go"},
		{outline = "#SKWHRAO*G", expected = "she won't go"},
		{outline = "#SWR-F", expected = "I've"},
		{outline = "#KWHR-FG", expected = "he's gone"},
		{outline = "#TWHAO-G", expected = "they'll go"},
	}

	for c in cases {
		test_expect_phrase_lookup(t, &phrasing, c.outline, .Verbs, c.expected)
	}

	test_expect_phrase_miss(t, &phrasing, "#SKWHR-BD", .Verbs)
}
