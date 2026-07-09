#+build windows
package stoin

PLATFORM_SERIAL_DEFAULT_BAUD :: 9600

Platform_Serial_Read_Result :: enum {
	Error,
	None,
	Byte,
}

Platform_Serial_Port :: struct {
	port_path: string,
	had_error: bool,
}

platform_serial_find_devices :: proc() -> [dynamic]string {
	return make([dynamic]string)
}

platform_serial_device_paths_destroy :: proc(paths: ^[dynamic]string) {
	for path in paths^ {
		owned_string_delete(path)
	}
	delete(paths^)
	paths^ = nil
}

platform_serial_find_device :: proc() -> (path: string, ok: bool) {
	return "", false
}

platform_serial_open :: proc(port: ^Platform_Serial_Port, port_path: string, baud_rate: int) -> bool {
	_ = port_path
	_ = baud_rate
	if port != nil {
		port^ = {had_error = true}
	}
	return false
}

platform_serial_close :: proc(port: ^Platform_Serial_Port) {
	if port == nil {
		return
	}
	owned_string_delete(port.port_path)
	port^ = {}
}

platform_serial_had_error :: proc(port: ^Platform_Serial_Port) -> bool {
	return port != nil && port.had_error
}

platform_serial_flush :: proc(port: ^Platform_Serial_Port) {
	_ = port
}

platform_serial_read_byte :: proc(port: ^Platform_Serial_Port, timeout_ms: uint) -> (value: byte, result: Platform_Serial_Read_Result) {
	_ = timeout_ms
	if port != nil {
		port.had_error = true
	}
	return 0, .Error
}

platform_serial_write_all :: proc(port: ^Platform_Serial_Port, bytes: []byte, timeout_ms: uint) -> bool {
	_ = bytes
	_ = timeout_ms
	if port != nil {
		port.had_error = true
	}
	return false
}
