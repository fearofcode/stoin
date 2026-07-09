package stoin

import "core:testing"

test_dictionary_stack_fixture :: proc(t: ^testing.T) -> Dictionary_Stack {
	stack: Dictionary_Stack
	dictionary_stack_init(&stack)
	paths := [?]string {
		"tests/test-dictionary.json",
		"tests/test-modal-dictionary.json",
		"tests/test-custom-dictionary.json",
	}
	enabled := [?]bool{true, false, true}
	testing.expect(t, dictionary_stack_set_paths(&stack, paths[:], enabled[:]))
	testing.expect(t, dictionary_stack_load(&stack))
	return stack
}

@(test)
test_dictionary_stack_loads_enabled_layers :: proc(t: ^testing.T) {
	stack := test_dictionary_stack_fixture(t)
	defer dictionary_stack_destroy(&stack)

	translation, found := dictionary_lookup_stroke(&stack.dictionary, "KAT")
	testing.expect(t, found)
	testing.expect_value(t, translation, "kitten")

	translation, found = dictionary_lookup_stroke(&stack.dictionary, "-R")
	testing.expect(t, found)
	testing.expect_value(t, translation, "=undo")

	translation, found = dictionary_lookup_stroke(&stack.dictionary, "STPH")
	testing.expect(t, found)
	testing.expect_value(t, translation, "{plover:toggle_dict:!test-modal-dictionary.json}")
}

@(test)
test_dictionary_stack_toggle_selection :: proc(t: ^testing.T) {
	stack := test_dictionary_stack_fixture(t)
	defer dictionary_stack_destroy(&stack)

	testing.expect(t, dictionary_stack_toggle(&stack, "!test-modal-dictionary.json"))

	translation, found := dictionary_lookup_stroke(&stack.dictionary, "-R")
	testing.expect(t, found)
	testing.expect_value(t, translation, "{#Left}{^}")

	testing.expect(t, dictionary_stack_toggle(&stack, "!test-modal-dictionary.json"))

	translation, found = dictionary_lookup_stroke(&stack.dictionary, "-R")
	testing.expect(t, found)
	testing.expect_value(t, translation, "=undo")
}
