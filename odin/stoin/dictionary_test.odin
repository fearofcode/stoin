package stoin

import "core:testing"

@(test)
test_dictionary_load_and_lookup :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)

	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))
	testing.expect(t, dictionary_count(&dictionary) > 0)
	testing.expect_value(t, dictionary_longest_key(&dictionary), 3)

	translation, ok := dictionary_lookup_stroke(&dictionary, "-T")
	testing.expect(t, ok)
	testing.expect_value(t, translation, "the")

	translation, ok = dictionary_lookup_stroke(&dictionary, "SA-P")
	testing.expect(t, ok)
	testing.expect_value(t, translation, "sap")

	translation, ok = dictionary_lookup_stroke(&dictionary, "TPH-T/PW-G")
	testing.expect(t, ok)
	testing.expect_value(t, translation, "in the beginning")

	in_the_bits, parsed := stroke_string_to_bits("TPH-T")
	testing.expect(t, parsed)
	strokes := [?]u64{in_the_bits}
	translation, ok = dictionary_lookup_strokes(&dictionary, strokes[:])
	testing.expect(t, ok)
	testing.expect_value(t, translation, "in the")
}

@(test)
test_dictionary_load_many_overrides :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)

	paths := [?]string{"tests/test-dictionary.json", "tests/test-custom-dictionary.json"}
	testing.expect(t, dictionary_load_many(&dictionary, paths[:]))

	translation, ok := dictionary_lookup_stroke(&dictionary, "KAT")
	testing.expect(t, ok)
	testing.expect_value(t, translation, "kitten")
}

@(test)
test_outline_canonicalization :: proc(t: ^testing.T) {
	buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
	n, stroke_count, ok := outline_to_canonical_key("#*-678G/SA-P", buffer[:])
	testing.expect(t, ok)
	testing.expect_value(t, stroke_count, 2)
	testing.expect_value(t, string(buffer[:n]), "#*FPLG/SAP")

	_, _, ok = outline_to_canonical_key("1", buffer[:])
	testing.expect(t, !ok)
}
