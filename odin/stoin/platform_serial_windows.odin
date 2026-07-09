#+build windows
package stoin

import "core:fmt"
import win "core:sys/windows"

foreign import stoin_windows_kernel32 "system:Kernel32.lib"

@(default_calling_convention="system")
foreign stoin_windows_kernel32 {
	QueryDosDeviceW :: proc(lpDeviceName: win.LPCWSTR, lpTargetPath: win.LPWSTR, ucchMax: win.DWORD) -> win.DWORD ---
	SetupComm :: proc(hFile: win.HANDLE, dwInQueue, dwOutQueue: win.DWORD) -> win.BOOL ---
	PurgeComm :: proc(hFile: win.HANDLE, dwFlags: win.DWORD) -> win.BOOL ---
}

PLATFORM_SERIAL_DEFAULT_BAUD :: 9600
WINDOWS_SERIAL_MAX_CANDIDATES :: 256
WINDOWS_MAXDWORD :: win.DWORD(0xffffffff)
WINDOWS_PURGE_TXCLEAR :: win.DWORD(0x0004)
WINDOWS_PURGE_RXCLEAR :: win.DWORD(0x0008)

Platform_Serial_Read_Result :: enum {
	Error,
	None,
	Byte,
}

Platform_Serial_Port :: struct {
	handle:    win.HANDLE,
	port_path: string,
	had_error: bool,
}

windows_serial_handle_valid :: proc(handle: win.HANDLE) -> bool {
	return handle != nil && handle != win.INVALID_HANDLE_VALUE
}

windows_serial_path_already_listed :: proc(paths: [dynamic]string, path: string) -> bool {
	for existing in paths {
		if existing == path {
			return true
		}
	}
	return false
}

windows_serial_add_path :: proc(paths: ^[dynamic]string, path: string) -> bool {
	if len(path) == 0 || windows_serial_path_already_listed(paths^, path) {
		return false
	}
	copy, ok := clone_string_ok(path)
	if !ok {
		return false
	}
	append(paths, copy)
	return true
}

windows_serial_is_com_name :: proc(path: string) -> bool {
	if len(path) < 4 {
		return false
	}
	if keymap_ascii_to_lower(path[0]) != 'c' ||
	   keymap_ascii_to_lower(path[1]) != 'o' ||
	   keymap_ascii_to_lower(path[2]) != 'm' {
		return false
	}
	for i := 3; i < len(path); i += 1 {
		if path[i] < '0' || path[i] > '9' {
			return false
		}
	}
	return true
}

windows_serial_path_has_device_prefix :: proc(path: string) -> bool {
	return len(path) >= 4 &&
		path[0] == '\\' &&
		path[1] == '\\' &&
		path[2] == '.' &&
		path[3] == '\\'
}

windows_serial_normalize_path :: proc(port_path: string) -> (string, bool) {
	if len(port_path) == 0 {
		return "", false
	}
	if windows_serial_path_has_device_prefix(port_path) {
		return clone_string_ok(port_path)
	}
	if windows_serial_is_com_name(port_path) {
		return fmt.aprintf("\\\\.\\%s", port_path), true
	}
	return clone_string_ok(port_path)
}

platform_serial_find_devices :: proc() -> [dynamic]string {
	paths := make([dynamic]string)
	target: [512]u16

	for i := 1; i <= WINDOWS_SERIAL_MAX_CANDIDATES; i += 1 {
		name := fmt.aprintf("COM%d", i)
		wname := win.utf8_to_wstring(name, context.temp_allocator)
		if wname != nil &&
		   QueryDosDeviceW(wname, raw_data(target[:]), win.DWORD(len(target))) != 0 {
			path := fmt.aprintf("\\\\.\\%s", name)
			if !windows_serial_add_path(&paths, path) {
				owned_string_delete(path)
			}
		}
		owned_string_delete(name)
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

windows_serial_configure_timeouts :: proc(handle: win.HANDLE, timeout_ms: uint) -> bool {
	timeouts: win.COMMTIMEOUTS
	timeouts.ReadIntervalTimeout = WINDOWS_MAXDWORD
	timeouts.ReadTotalTimeoutConstant = win.DWORD(timeout_ms)
	timeouts.WriteTotalTimeoutConstant = win.DWORD(timeout_ms)
	return win.SetCommTimeouts(handle, &timeouts) != win.FALSE
}

windows_serial_configure :: proc(handle: win.HANDLE, baud_rate: int) -> bool {
	dcb: win.DCB
	dcb.DCBlength = win.DWORD(size_of(win.DCB))
	if win.GetCommState(handle, &dcb) == win.FALSE {
		return false
	}

	dcb.BaudRate = win.DWORD(baud_rate)
	dcb.ByteSize = win.BYTE(8)
	dcb.Parity = .None
	dcb.StopBits = .One
	dcb.fBinary = true
	dcb.fDtrControl = .Enable
	dcb.fRtsControl = .Enable
	dcb.fOutxCtsFlow = false
	dcb.fOutxDsrFlow = false
	dcb.fOutX = false
	dcb.fInX = false

	if win.SetCommState(handle, &dcb) == win.FALSE ||
	   SetupComm(handle, 4096, 4096) == win.FALSE ||
	   !windows_serial_configure_timeouts(handle, 100) ||
	   PurgeComm(handle, WINDOWS_PURGE_RXCLEAR | WINDOWS_PURGE_TXCLEAR) == win.FALSE {
		return false
	}
	return true
}

platform_serial_open :: proc(port: ^Platform_Serial_Port, port_path: string, baud_rate: int) -> bool {
	if port == nil || len(port_path) == 0 {
		return false
	}
	port^ = {}
	port.handle = win.INVALID_HANDLE_VALUE

	normalized_path, normalized_ok := windows_serial_normalize_path(port_path)
	if !normalized_ok {
		return false
	}
	success := false
	defer if !success {
		owned_string_delete(normalized_path)
	}

	wpath := win.utf8_to_wstring(normalized_path, context.temp_allocator)
	if wpath == nil {
		return false
	}

	handle := win.CreateFileW(
		wpath,
		win.GENERIC_READ | win.GENERIC_WRITE,
		0,
		nil,
		win.OPEN_EXISTING,
		win.FILE_ATTRIBUTE_NORMAL,
		nil,
	)
	if !windows_serial_handle_valid(handle) {
		return false
	}

	if !windows_serial_configure(handle, baud_rate) {
		_ = win.CloseHandle(handle)
		return false
	}

	port.handle = handle
	port.port_path = normalized_path
	success = true
	return true
}

platform_serial_close :: proc(port: ^Platform_Serial_Port) {
	if port == nil {
		return
	}
	if windows_serial_handle_valid(port.handle) {
		_ = win.CloseHandle(port.handle)
	}
	owned_string_delete(port.port_path)
	port^ = {}
}

platform_serial_had_error :: proc(port: ^Platform_Serial_Port) -> bool {
	return port != nil && port.had_error
}

platform_serial_flush :: proc(port: ^Platform_Serial_Port) {
	if port == nil || !windows_serial_handle_valid(port.handle) {
		return
	}
	_ = PurgeComm(port.handle, WINDOWS_PURGE_RXCLEAR | WINDOWS_PURGE_TXCLEAR)
}

platform_serial_read_byte :: proc(port: ^Platform_Serial_Port, timeout_ms: uint) -> (value: byte, result: Platform_Serial_Read_Result) {
	if port == nil || !windows_serial_handle_valid(port.handle) {
		if port != nil {
			port.had_error = true
		}
		return 0, .Error
	}

	if !windows_serial_configure_timeouts(port.handle, timeout_ms) {
		port.had_error = true
		return 0, .Error
	}

	bytes_read: win.DWORD
	if win.ReadFile(port.handle, rawptr(&value), 1, &bytes_read, nil) == win.FALSE {
		port.had_error = true
		return 0, .Error
	}
	if bytes_read == 1 {
		return value, .Byte
	}
	return 0, .None
}

platform_serial_write_all :: proc(port: ^Platform_Serial_Port, bytes: []byte, timeout_ms: uint) -> bool {
	if port == nil || !windows_serial_handle_valid(port.handle) {
		if port != nil {
			port.had_error = true
		}
		return false
	}
	if len(bytes) == 0 {
		return true
	}

	if !windows_serial_configure_timeouts(port.handle, timeout_ms) {
		port.had_error = true
		return false
	}

	written_count := 0
	for written_count < len(bytes) {
		remaining := len(bytes) - written_count
		request_count := remaining
		if request_count > int(max(u32)) {
			request_count = int(max(u32))
		}

		bytes_written: win.DWORD
		if win.WriteFile(
			port.handle,
			rawptr(&bytes[written_count]),
			win.DWORD(request_count),
			&bytes_written,
			nil,
		) == win.FALSE {
			port.had_error = true
			return false
		}
		if bytes_written == 0 {
			port.had_error = true
			return false
		}
		written_count += int(bytes_written)
	}
	return true
}
