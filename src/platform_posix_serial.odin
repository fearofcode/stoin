#+build darwin, linux
package stoin

import "core:c"
import "core:strings"
import "core:sys/posix"

when ODIN_OS == .Darwin {
	foreign import platform_serial_lib "system:System"
} else {
	foreign import platform_serial_lib "system:c"
}

foreign platform_serial_lib {
	@(link_name="cfmakeraw")
	platform_cfmakeraw :: proc(termios_p: ^posix.termios) ---
}

PLATFORM_SERIAL_DEFAULT_BAUD :: 9600
PLATFORM_SERIAL_CS8 :: posix.CSIZE

when ODIN_OS == .Darwin {
	PLATFORM_SERIAL_DARWIN_CRTSCTS :: transmute(posix.CControl_Flags)posix.tcflag_t(0x00030000)
}

Platform_Serial_Read_Result :: enum {
	Error,
	None,
	Byte,
}

Platform_Serial_Port :: struct {
	fd:        posix.FD,
	port_path: string,
	had_error: bool,
}

platform_serial_baud_to_speed :: proc(baud_rate: int) -> (speed: posix.speed_t, ok: bool) {
	switch baud_rate {
	case 300:
		return .B300, true
	case 600:
		return .B600, true
	case 1200:
		return .B1200, true
	case 2400:
		return .B2400, true
	case 4800:
		return .B4800, true
	case 9600:
		return .B9600, true
	case 19200:
		return .B19200, true
	case 38400:
		return .B38400, true
	}
	return posix.speed_t(0), false
}

platform_serial_path_already_listed :: proc(paths: [dynamic]string, path: string) -> bool {
	for existing in paths {
		if existing == path {
			return true
		}
	}
	return false
}

platform_serial_add_path :: proc(paths: ^[dynamic]string, path: string) -> bool {
	if len(path) == 0 || platform_serial_path_already_listed(paths^, path) {
		return false
	}
	copy, ok := clone_string_ok(path)
	if !ok {
		return false
	}
	append(paths, copy)
	return true
}

platform_serial_add_glob_matches :: proc(paths: ^[dynamic]string, pattern: string) {
	cpattern, cpattern_err := strings.clone_to_cstring(pattern)
	if cpattern_err != nil {
		return
	}
	defer delete(cpattern)

	matches: posix.glob_t
	glob_result := posix.glob(cpattern, {}, nil, &matches)
	if glob_result != .SUCCESS {
		posix.globfree(&matches)
		return
	}
	defer posix.globfree(&matches)

	for i := 0; i < int(matches.gl_pathc); i += 1 {
		path, path_err := strings.clone_from_cstring(matches.gl_pathv[i])
		if path_err != nil {
			continue
		}
		if !platform_serial_add_path(paths, path) {
			owned_string_delete(path)
		}
	}
}

platform_serial_find_devices :: proc() -> [dynamic]string {
	paths := make([dynamic]string)

	when ODIN_OS == .Darwin {
		patterns := [?]string {
			"/dev/cu.usbmodem*",
			"/dev/cu.usbserial*",
			"/dev/cu.SLAB_USBtoUART*",
			"/dev/cu.wchusbserial*",
			"/dev/cu.KeySerial*",
		}
		for pattern in patterns {
			platform_serial_add_glob_matches(&paths, pattern)
		}
	} else when ODIN_OS == .Linux {
		patterns := [?]string {
			"/dev/serial/by-id/*",
			"/dev/ttyACM*",
			"/dev/ttyUSB*",
			"/dev/ttyAMA*",
			"/dev/rfcomm*",
		}
		for pattern in patterns {
			platform_serial_add_glob_matches(&paths, pattern)
		}
	}

	return paths
}

platform_serial_device_paths_destroy :: proc(paths: ^[dynamic]string) {
	for path in paths^ {
		owned_string_delete(path)
	}
	delete(paths^)
	paths^ = nil
}

platform_serial_find_device :: proc() -> (path: string, ok: bool) {
	paths := platform_serial_find_devices()
	defer platform_serial_device_paths_destroy(&paths)
	if len(paths) == 0 {
		return "", false
	}
	return clone_string_ok(paths[0])
}

platform_serial_configure :: proc(fd: posix.FD, baud_rate: int) -> bool {
	options: posix.termios
	if posix.tcgetattr(fd, &options) != .OK {
		return false
	}

	speed, speed_ok := platform_serial_baud_to_speed(baud_rate)
	if !speed_ok {
		posix.set_errno(.EINVAL)
		return false
	}

	platform_cfmakeraw(&options)
	options.c_cflag -= posix.CSIZE
	options.c_cflag -= {.PARENB, .CSTOPB}
	options.c_cflag += PLATFORM_SERIAL_CS8
	options.c_cflag += {.CLOCAL, .CREAD}
	when ODIN_OS == .Darwin {
		options.c_cflag -= PLATFORM_SERIAL_DARWIN_CRTSCTS
	}
	options.c_cc[.VMIN] = posix.cc_t(0)
	options.c_cc[.VTIME] = posix.cc_t(1)

	if posix.cfsetispeed(&options, speed) != .OK || posix.cfsetospeed(&options, speed) != .OK {
		return false
	}
	if posix.tcsetattr(fd, .TCSANOW, &options) != .OK {
		return false
	}
	posix.tcflush(fd, .TCIOFLUSH)
	return true
}

platform_serial_open :: proc(port: ^Platform_Serial_Port, port_path: string, baud_rate: int) -> bool {
	if port == nil || len(port_path) == 0 {
		posix.set_errno(.ENODEV)
		return false
	}
	port^ = {}
	port.fd = posix.FD(-1)

	cpath, cpath_err := strings.clone_to_cstring(port_path)
	if cpath_err != nil {
		posix.set_errno(.ENOMEM)
		return false
	}
	defer delete(cpath)

	fd := posix.open(cpath, {.RDWR, .NOCTTY, .NONBLOCK})
	if c.int(fd) < 0 {
		return false
	}

	if !platform_serial_configure(fd, baud_rate) {
		posix.close(fd)
		return false
	}

	path_copy, path_ok := clone_string_ok(port_path)
	if !path_ok {
		posix.close(fd)
		posix.set_errno(.ENOMEM)
		return false
	}

	port.fd = fd
	port.port_path = path_copy
	return true
}

platform_serial_close :: proc(port: ^Platform_Serial_Port) {
	if port == nil {
		return
	}
	if c.int(port.fd) >= 0 {
		posix.close(port.fd)
	}
	owned_string_delete(port.port_path)
	port^ = {}
}

platform_serial_had_error :: proc(port: ^Platform_Serial_Port) -> bool {
	return port != nil && port.had_error
}

platform_serial_flush :: proc(port: ^Platform_Serial_Port) {
	if port == nil || c.int(port.fd) < 0 {
		return
	}
	posix.tcflush(port.fd, .TCIOFLUSH)
}

platform_serial_read_byte :: proc(port: ^Platform_Serial_Port, timeout_ms: uint) -> (value: byte, result: Platform_Serial_Read_Result) {
	if port == nil || c.int(port.fd) < 0 {
		if port != nil {
			port.had_error = true
		}
		posix.set_errno(.EBADF)
		return 0, .Error
	}

	read_fds: posix.fd_set
	posix.FD_ZERO(&read_fds)
	posix.FD_SET(port.fd, &read_fds)

	timeout := posix.timeval {
		tv_sec = posix.time_t(timeout_ms / 1000),
		tv_usec = posix.suseconds_t((timeout_ms % 1000) * 1000),
	}
	ready := posix.select(c.int(port.fd) + 1, &read_fds, nil, nil, &timeout)
	if ready < 0 {
		if posix.errno() == .EINTR {
			return 0, .None
		}
		port.had_error = true
		return 0, .Error
	}
	if ready == 0 {
		return 0, .None
	}

	buffer: [1]byte
	bytes_read := posix.read(port.fd, raw_data(buffer[:]), 1)
	if bytes_read == 1 {
		return buffer[0], .Byte
	}
	err := posix.errno()
	if bytes_read == 0 || err == .EAGAIN || err == .EWOULDBLOCK || err == .EINTR {
		return 0, .None
	}

	port.had_error = true
	return 0, .Error
}

platform_serial_write_all :: proc(port: ^Platform_Serial_Port, bytes: []byte, timeout_ms: uint) -> bool {
	if port == nil || c.int(port.fd) < 0 {
		if port != nil {
			port.had_error = true
		}
		posix.set_errno(.EBADF)
		return false
	}

	written_count := 0
	byte_count := int(len(bytes))
	for written_count < byte_count {
		write_fds: posix.fd_set
		posix.FD_ZERO(&write_fds)
		posix.FD_SET(port.fd, &write_fds)

		timeout := posix.timeval {
			tv_sec = posix.time_t(timeout_ms / 1000),
			tv_usec = posix.suseconds_t((timeout_ms % 1000) * 1000),
		}
		ready := posix.select(c.int(port.fd) + 1, nil, &write_fds, nil, &timeout)
		if ready < 0 {
			if posix.errno() == .EINTR {
				continue
			}
			port.had_error = true
			return false
		}
		if ready == 0 {
			posix.set_errno(.ETIMEDOUT)
			return false
		}

		bytes_written := posix.write(port.fd, raw_data(bytes[written_count:]), c.size_t(byte_count - written_count))
		if bytes_written > 0 {
			written_count += int(bytes_written)
			continue
		}
		err := posix.errno()
		if bytes_written == 0 || err == .EAGAIN || err == .EWOULDBLOCK || err == .EINTR {
			continue
		}

		port.had_error = true
		return false
	}

	return true
}
