package stoin

import "core:os"
import "core:strings"
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
test_dictionary_dump_json :: proc(t: ^testing.T) {
	mkdir_err := os.make_directory("build")
	testing.expect(t, mkdir_err == nil || mkdir_err == .Exist)

	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	path := "build/odin-test-dictionary-dump.json"
	defer os.remove(path)
	testing.expect(t, dictionary_dump_json(&dictionary, path))

	data, read_err := os.read_entire_file(path, context.allocator)
	testing.expect(t, read_err == nil)
	defer delete(data)
	dump := string(data)
	testing.expect(t, strings.contains(dump, "\"STOER/Z\""))
	testing.expect(t, strings.contains(dump, "\"stories\""))
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

@(test)
test_dictionary_find_translation_outline :: proc(t: ^testing.T) {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)
	testing.expect(t, dictionary_load(&dictionary, "tests/test-dictionary.json"))

	outline, found := dictionary_find_translation_outline(&dictionary, "in the", "TPH/-T", 1)
	testing.expect(t, found)
	testing.expect_value(t, outline, "TPH-T")
	owned_string_delete(outline)

	outline, found = dictionary_find_translation_outline(&dictionary, "quickly", "KWEUBG/-L", 1)
	testing.expect(t, found)
	testing.expect_value(t, outline, "KWEUL")
	owned_string_delete(outline)

	outline, found = dictionary_find_translation_outline(&dictionary, "in the", "TPH-T", 1)
	testing.expect(t, !found)
	owned_string_delete(outline)
}
