package stoin

import "core:testing"

@(test)
test_macos_keycode_to_us_qwerty_printable :: proc(t: ^testing.T) {
	testing.expect_value(t, macos_keycode_to_us_qwerty_printable(0), byte('a'))
	testing.expect_value(t, macos_keycode_to_us_qwerty_printable(49), byte(' '))
	testing.expect_value(t, macos_keycode_to_us_qwerty_printable(105), byte(0))
}
