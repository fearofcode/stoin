package stoin

import "core:testing"

@(test)
test_linux_ioctl_request_values_match_uapi :: proc(t: ^testing.T) {
	testing.expect_value(t, LINUX_C_INT_SIZE, u32(4))
	testing.expect_value(t, LINUX_UI_DEV_CREATE, 0x00005501)
	testing.expect_value(t, LINUX_UI_DEV_DESTROY, 0x00005502)
	testing.expect_value(t, size_of(Linux_Uinput_Setup), 92)
	testing.expect_value(t, LINUX_UI_DEV_SETUP, 0x405c5503)
	testing.expect_value(t, LINUX_UI_SET_EVBIT, 0x40045564)
	testing.expect_value(t, LINUX_UI_SET_KEYBIT, 0x40045565)
	testing.expect_value(t, LINUX_EVIOCGRAB, 0x40044590)
}
