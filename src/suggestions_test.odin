package stoin

import "core:testing"

@(test)
test_brevity_suggestion_log_line :: proc(t: ^testing.T) {
	suggestion := Brevity_Suggestion {
		suggested_outline = "TPH-T",
		typed_outline = "TPH/-T",
		text = "in the",
		typed_strokes = 2,
		suggested_strokes = 1,
		saved_strokes = 1,
	}
	line, ok := brevity_suggestion_log_line(&suggestion, 123)
	defer owned_string_delete(line)

	testing.expect(t, ok)
	testing.expect_value(t, line, `{"unix_time":123,"suggested_outline":"TPH-T","typed_outline":"TPH/-T","text":"in the","typed_strokes":2,"suggested_strokes":1,"saved_strokes":1}`)
}
