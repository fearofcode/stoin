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
