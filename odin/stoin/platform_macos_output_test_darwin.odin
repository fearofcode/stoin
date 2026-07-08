package stoin

import "core:testing"

@(test)
test_macos_utf8_to_utf16 :: proc(t: ^testing.T) {
	units := macos_utf8_to_utf16("a🙂")
	defer delete(units)

	testing.expect_value(t, len(units), 3)
	testing.expect_value(t, units[0], u16('a'))
	testing.expect_value(t, units[1], u16(0xd83d))
	testing.expect_value(t, units[2], u16(0xde42))
}

@(test)
test_macos_parse_key_combination :: proc(t: ^testing.T) {
	keycode, flags, ok := macos_parse_key_combination("Control(Shift(Right))")
	testing.expect(t, ok)
	testing.expect_value(t, keycode, CGKeyCode(124))
	testing.expect(t, (flags & KCG_EVENT_FLAG_MASK_CONTROL) != 0)
	testing.expect(t, (flags & KCG_EVENT_FLAG_MASK_SHIFT) != 0)

	keycode, flags, ok = macos_parse_key_combination("command(f)")
	testing.expect(t, ok)
	testing.expect_value(t, keycode, CGKeyCode(3))
	testing.expect_value(t, flags, KCG_EVENT_FLAG_MASK_COMMAND)

	_, _, ok = macos_parse_key_combination("control(nope)")
	testing.expect(t, !ok)
}
