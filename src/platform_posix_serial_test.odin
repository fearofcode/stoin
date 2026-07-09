#+build darwin, linux
package stoin

import "core:testing"
import "core:sys/posix"

@(test)
test_platform_serial_baud_to_speed :: proc(t: ^testing.T) {
	speed, ok := platform_serial_baud_to_speed(9600)
	testing.expect(t, ok)
	testing.expect_value(t, speed, posix.speed_t.B9600)

	_, ok = platform_serial_baud_to_speed(12345)
	testing.expect(t, !ok)
}

@(test)
test_platform_serial_add_path_deduplicates :: proc(t: ^testing.T) {
	paths := make([dynamic]string)
	defer platform_serial_device_paths_destroy(&paths)

	testing.expect(t, platform_serial_add_path(&paths, "/dev/cu.usbserial-test"))
	testing.expect(t, !platform_serial_add_path(&paths, "/dev/cu.usbserial-test"))
	testing.expect_value(t, len(paths), 1)
	testing.expect_value(t, paths[0], "/dev/cu.usbserial-test")
}
