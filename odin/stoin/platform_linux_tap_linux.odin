#+build linux
package stoin

import "base:runtime"
import "core:fmt"
import "core:mem"
import "core:strings"
import "core:sync"
import "core:sys/linux"
import "core:sys/posix"
import "core:thread"

LINUX_INPUT_DEVICE_GLOB :: "/dev/input/event*"
LINUX_EV_MAX :: 0x1f
LINUX_BITS_PER_ULONG :: 64
LINUX_EV_BITS_LENGTH :: LINUX_EV_MAX / LINUX_BITS_PER_ULONG + 1
LINUX_KEY_BITS_LENGTH :: LINUX_KEY_MAX / LINUX_BITS_PER_ULONG + 1
LINUX_EVDEV_IOCTL_BASE :: byte('E')
LINUX_EVDEV_IOC_READ :: u32(2)
LINUX_EVDEV_IOC_WRITE :: u32(1)

Linux_Keyboard_Device :: struct {
	fd:   linux.Fd,
	path: string,
	name: [256]byte,
}

Linux_Keyboard_Open_Result :: enum {
	Added,
	Permission_Denied,
	Open_Failed,
	Not_Keyboard,
	Grab_Failed,
	Allocation_Failed,
}

Linux_Keyboard_Open_Report :: struct {
	path_count: int,
	added_count: int,
	permission_denied_count: int,
	open_failed_count: int,
	non_keyboard_count: int,
	grab_failed_count: int,
	allocation_failed_count: int,
}

linux_keyboards: [dynamic]Linux_Keyboard_Device
linux_keyboard_owner: ^Steno_Runtime_Owner
linux_keyboard_grab_devices: bool
linux_keyboard_shift_down: bool
linux_keyboard_control_down: bool
linux_keyboard_option_down: bool
linux_keyboard_command_down: bool
linux_keyboard_running: bool
linux_keyboard_thread: ^thread.Thread
linux_keyboard_thread_mutex: sync.Mutex
linux_keyboard_thread_condition: sync.Cond
linux_keyboard_thread_ready: bool
linux_keyboard_thread_ok: bool

linux_evdev_ioc :: proc(dir, nr, size: u32) -> u32 {
	return linux_ioc(dir, u32(LINUX_EVDEV_IOCTL_BASE), nr, size)
}

linux_eviocgname :: proc(length: int) -> u32 {
	return linux_evdev_ioc(LINUX_EVDEV_IOC_READ, 0x06, u32(length))
}

linux_eviocgbit :: proc(event_type, length: int) -> u32 {
	return linux_evdev_ioc(LINUX_EVDEV_IOC_READ, u32(0x20 + event_type), u32(length))
}

LINUX_EVIOCGRAB :: (LINUX_EVDEV_IOC_WRITE << 30) |
	(u32(LINUX_EVDEV_IOCTL_BASE) << 8) |
	0x90 |
	(u32(size_of(int)) << 16)

linux_bit_word :: proc(bit: int) -> int {
	return bit / LINUX_BITS_PER_ULONG
}

linux_bit_mask :: proc(bit: int) -> u64 {
	return u64(1) << uint(bit % LINUX_BITS_PER_ULONG)
}

linux_test_bit :: proc(bit: int, bits: []u64) -> bool {
	word := linux_bit_word(bit)
	return word >= 0 && word < len(bits) && (bits[word] & linux_bit_mask(bit)) != 0
}

linux_ioctl_pointer_ok :: proc(fd: linux.Fd, request: u32, ptr: rawptr) -> bool {
	return linux_ioctl_ok(fd, request, uintptr(ptr))
}

linux_ioctl_int_ok :: proc(fd: linux.Fd, request: u32, value: int) -> bool {
	return linux_ioctl_ok(fd, request, uintptr(value))
}

linux_input_device_name_is_stoin :: proc(name: []byte) -> bool {
	stoin_name := LINUX_UINPUT_NAME
	for i in 0..<len(stoin_name) {
		if i >= len(name) || name[i] != stoin_name[i] {
			return false
		}
	}
	return len(name) == len(stoin_name) || name[len(stoin_name)] == 0
}

linux_input_device_is_keyboard :: proc(fd: linux.Fd) -> bool {
	ev_bits: [LINUX_EV_BITS_LENGTH]u64
	if !linux_ioctl_pointer_ok(fd, linux_eviocgbit(0, size_of(ev_bits)), rawptr(&ev_bits[0])) {
		return false
	}
	if !linux_test_bit(LINUX_EV_KEY, ev_bits[:]) {
		return false
	}

	key_bits: [LINUX_KEY_BITS_LENGTH]u64
	if !linux_ioctl_pointer_ok(fd, linux_eviocgbit(LINUX_EV_KEY, size_of(key_bits)), rawptr(&key_bits[0])) {
		return false
	}
	return linux_test_bit(LINUX_KEY_A, key_bits[:]) &&
		linux_test_bit(LINUX_KEY_SPACE, key_bits[:]) &&
		linux_test_bit(LINUX_KEY_ENTER, key_bits[:])
}

linux_logical_keycode_from_evdev :: proc(evdev: int) -> (u16, bool) {
	switch evdev {
	case LINUX_KEY_A: return 0, true
	case LINUX_KEY_S: return 1, true
	case LINUX_KEY_D: return 2, true
	case LINUX_KEY_F: return 3, true
	case LINUX_KEY_H: return 4, true
	case LINUX_KEY_G: return 5, true
	case LINUX_KEY_Z: return 6, true
	case LINUX_KEY_X: return 7, true
	case LINUX_KEY_C: return 8, true
	case LINUX_KEY_V: return 9, true
	case LINUX_KEY_B: return 11, true
	case LINUX_KEY_Q: return 12, true
	case LINUX_KEY_W: return 13, true
	case LINUX_KEY_E: return 14, true
	case LINUX_KEY_R: return 15, true
	case LINUX_KEY_Y: return 16, true
	case LINUX_KEY_T: return 17, true
	case LINUX_KEY_1: return 18, true
	case LINUX_KEY_2: return 19, true
	case LINUX_KEY_3: return 20, true
	case LINUX_KEY_4: return 21, true
	case LINUX_KEY_6: return 22, true
	case LINUX_KEY_5: return 23, true
	case LINUX_KEY_9: return 25, true
	case LINUX_KEY_7: return 26, true
	case LINUX_KEY_8: return 28, true
	case LINUX_KEY_0: return 29, true
	case LINUX_KEY_RIGHTBRACE: return 30, true
	case LINUX_KEY_O: return 31, true
	case LINUX_KEY_U: return 32, true
	case LINUX_KEY_LEFTBRACE: return 33, true
	case LINUX_KEY_I: return 34, true
	case LINUX_KEY_P: return 35, true
	case LINUX_KEY_ENTER: return 36, true
	case LINUX_KEY_L: return 37, true
	case LINUX_KEY_J: return 38, true
	case LINUX_KEY_APOSTROPHE: return 39, true
	case LINUX_KEY_K: return 40, true
	case LINUX_KEY_SEMICOLON: return 41, true
	case LINUX_KEY_BACKSLASH: return 42, true
	case LINUX_KEY_COMMA: return 43, true
	case LINUX_KEY_SLASH: return 44, true
	case LINUX_KEY_N: return 45, true
	case LINUX_KEY_M: return 46, true
	case LINUX_KEY_DOT: return 47, true
	case LINUX_KEY_TAB: return 48, true
	case LINUX_KEY_SPACE: return 49, true
	case LINUX_KEY_GRAVE: return 50, true
	case LINUX_KEY_BACKSPACE: return 51, true
	case LINUX_KEY_ESC: return 53, true
	case LINUX_KEY_RIGHTMETA: return 54, true
	case LINUX_KEY_LEFTMETA: return 55, true
	case LINUX_KEY_LEFTSHIFT: return 56, true
	case LINUX_KEY_LEFTALT: return 58, true
	case LINUX_KEY_LEFTCTRL: return 59, true
	case LINUX_KEY_RIGHTSHIFT: return 60, true
	case LINUX_KEY_RIGHTALT: return 61, true
	case LINUX_KEY_RIGHTCTRL: return 62, true
	case LINUX_KEY_F17: return 64, true
	case LINUX_KEY_F18: return 79, true
	case LINUX_KEY_F19: return 80, true
	case LINUX_KEY_F20: return 90, true
	case LINUX_KEY_F13: return 105, true
	case LINUX_KEY_F16: return 106, true
	case LINUX_KEY_F14: return 107, true
	case LINUX_KEY_F15: return 113, true
	}
	return 0, false
}

linux_printable_from_logical :: proc(logical: u16) -> byte {
	switch logical {
	case 0: return 'a'
	case 1: return 's'
	case 2: return 'd'
	case 3: return 'f'
	case 4: return 'h'
	case 5: return 'g'
	case 6: return 'z'
	case 7: return 'x'
	case 8: return 'c'
	case 9: return 'v'
	case 11: return 'b'
	case 12: return 'q'
	case 13: return 'w'
	case 14: return 'e'
	case 15: return 'r'
	case 16: return 'y'
	case 17: return 't'
	case 18: return '1'
	case 19: return '2'
	case 20: return '3'
	case 21: return '4'
	case 22: return '6'
	case 23: return '5'
	case 25: return '9'
	case 26: return '7'
	case 28: return '8'
	case 29: return '0'
	case 30: return ']'
	case 31: return 'o'
	case 32: return 'u'
	case 33: return '['
	case 34: return 'i'
	case 35: return 'p'
	case 37: return 'l'
	case 38: return 'j'
	case 39: return '\''
	case 40: return 'k'
	case 41: return ';'
	case 42: return '\\'
	case 43: return ','
	case 44: return '/'
	case 45: return 'n'
	case 46: return 'm'
	case 47: return '.'
	case 49: return ' '
	case 50: return '`'
	}
	return 0
}

linux_update_modifier_state :: proc(logical: u16, is_down: bool) {
	switch logical {
	case KEYCODE_LEFT_COMMAND, KEYCODE_RIGHT_COMMAND:
		linux_keyboard_command_down = is_down
	case 56, 60:
		linux_keyboard_shift_down = is_down
	case KEYCODE_LEFT_OPTION, KEYCODE_RIGHT_OPTION:
		linux_keyboard_option_down = is_down
	case KEYCODE_LEFT_CONTROL, KEYCODE_RIGHT_CONTROL:
		linux_keyboard_control_down = is_down
	}
}

linux_keyboard_add_device :: proc(path: string, grab: bool) -> Linux_Keyboard_Open_Result {
	if len(path) == 0 {
		return .Open_Failed
	}

	cpath, cpath_err := strings.clone_to_cstring(path)
	if cpath_err != nil {
		return .Allocation_Failed
	}
	defer delete(cpath)

	fd, open_err := linux.open(cpath, {.NONBLOCK, .CLOEXEC})
	if open_err != .NONE {
		if open_err == .EACCES || open_err == .EPERM {
			return .Permission_Denied
		}
		return .Open_Failed
	}

	name: [256]byte
	_ = linux_ioctl_pointer_ok(fd, linux_eviocgname(len(name)), rawptr(&name[0]))
	if linux_input_device_name_is_stoin(name[:]) || !linux_input_device_is_keyboard(fd) {
		_ = linux.close(fd)
		return .Not_Keyboard
	}

	if grab && !linux_ioctl_int_ok(fd, LINUX_EVIOCGRAB, 1) {
		fmt.eprintln("stoin: warning: failed to grab Linux keyboard device", path)
		_ = linux.close(fd)
		return .Grab_Failed
	}

	path_copy, path_ok := clone_string_ok(path)
	if !path_ok {
		if grab {
			_ = linux_ioctl_int_ok(fd, LINUX_EVIOCGRAB, 0)
		}
		_ = linux.close(fd)
		return .Allocation_Failed
	}

	device := Linux_Keyboard_Device {
		fd = fd,
		path = path_copy,
		name = name,
	}
	append(&linux_keyboards, device)
	return .Added
}

linux_keyboard_close_devices :: proc() {
	for &device in linux_keyboards {
		if int(device.fd) >= 0 {
			if linux_keyboard_grab_devices {
				_ = linux_ioctl_int_ok(device.fd, LINUX_EVIOCGRAB, 0)
			}
			_ = linux.close(device.fd)
		}
		owned_string_delete(device.path)
	}
	delete(linux_keyboards)
	linux_keyboards = nil
}

linux_keyboard_print_open_report :: proc(report: ^Linux_Keyboard_Open_Report) {
	if report == nil {
		return
	}
	fmt.eprintf(
		"stoin: checked %d Linux input event devices: %d opened, %d permission denied, %d open failed, %d not keyboards, %d grab failed\n",
		report.path_count,
		report.added_count,
		report.permission_denied_count,
		report.open_failed_count,
		report.non_keyboard_count,
		report.grab_failed_count,
	)
	if report.permission_denied_count > 0 {
		fmt.eprintln("stoin: qwerty capture needs read/grab access to keyboard devices under /dev/input/event*")
		fmt.eprintln("stoin: see docs/linux-setup.md for the stoin input-device udev rule")
	}
}

linux_keyboard_open_devices :: proc(grab: bool) -> bool {
	cpattern, cpattern_err := strings.clone_to_cstring(LINUX_INPUT_DEVICE_GLOB)
	if cpattern_err != nil {
		return false
	}
	defer delete(cpattern)

	matches: posix.glob_t
	glob_result := posix.glob(cpattern, {}, nil, &matches)
	if glob_result != .SUCCESS {
		posix.globfree(&matches)
		fmt.eprintln("stoin: no Linux input event devices found under /dev/input/event*")
		return false
	}
	defer posix.globfree(&matches)

	report := Linux_Keyboard_Open_Report {
		path_count = int(matches.gl_pathc),
	}
	for i := 0; i < int(matches.gl_pathc); i += 1 {
		path, path_err := strings.clone_from_cstring(matches.gl_pathv[i])
		if path_err != nil {
			report.allocation_failed_count += 1
			continue
		}
		switch linux_keyboard_add_device(path, grab) {
		case .Added:
			report.added_count += 1
		case .Permission_Denied:
			report.permission_denied_count += 1
		case .Open_Failed:
			report.open_failed_count += 1
		case .Not_Keyboard:
			report.non_keyboard_count += 1
		case .Grab_Failed:
			report.grab_failed_count += 1
		case .Allocation_Failed:
			report.allocation_failed_count += 1
		}
		owned_string_delete(path)
	}

	if len(linux_keyboards) == 0 {
		linux_keyboard_print_open_report(&report)
	}
	return len(linux_keyboards) > 0
}

linux_keyboard_process_key_event :: proc(event: ^Linux_Input_Event) {
	if event == nil || event.type != u16(LINUX_EV_KEY) {
		return
	}

	logical, mapped := linux_logical_keycode_from_evdev(int(event.code))
	is_down := event.value != 0
	is_repeat := event.value == 2

	if mapped && !is_repeat {
		linux_update_modifier_state(logical, is_down)
	}

	consumed := false
	if mapped && linux_keyboard_owner != nil {
		input := Input_Event {
			keycode = logical,
			is_down = is_down,
			is_repeat = is_repeat,
			shift = linux_keyboard_shift_down,
			control = linux_keyboard_control_down,
			option = linux_keyboard_option_down,
			command = linux_keyboard_command_down,
			printable = linux_printable_from_logical(logical),
		}
		consumed = steno_runtime_owner_handle_event(linux_keyboard_owner, input)
	}

	if linux_keyboard_grab_devices && !consumed {
		_ = linux_uinput_emit_key_event(int(event.code), int(event.value))
	}
}

linux_keyboard_process_device :: proc(device: ^Linux_Keyboard_Device) {
	if device == nil || int(device.fd) < 0 {
		return
	}

	for {
		event: Linux_Input_Event
		data := mem.ptr_to_bytes(&event)
		bytes_read, err := linux.read(device.fd, data)
		if err == .NONE && bytes_read == len(data) {
			linux_keyboard_process_key_event(&event)
			continue
		}
		if err == .EINTR {
			continue
		}
		if err == .EAGAIN || err == .EWOULDBLOCK {
			break
		}
		break
	}
}

linux_keyboard_poll_once :: proc(timeout_ms: int) -> bool {
	if len(linux_keyboards) == 0 {
		if timeout_ms != 0 {
			linux_sleep_ms(250)
		}
		return false
	}

	fds := make([]linux.Poll_Fd, len(linux_keyboards), context.temp_allocator)
	for i in 0..<len(linux_keyboards) {
		fds[i] = linux.Poll_Fd {
			fd = linux_keyboards[i].fd,
			events = {.IN},
		}
	}

	timeout := timeout_ms
	if timeout < 0 {
		timeout = 250
	}
	ready, err := linux.poll(fds, i32(timeout))
	if err == .EINTR || ready <= 0 {
		return false
	}
	if err != .NONE {
		return false
	}

	made_progress := false
	for i in 0..<len(linux_keyboards) {
		if .IN in fds[i].revents {
			linux_keyboard_process_device(&linux_keyboards[i])
			made_progress = true
		}
	}
	return made_progress
}

linux_keyboard_start :: proc(owner: ^Steno_Runtime_Owner, grab: bool) -> bool {
	if owner == nil {
		return false
	}
	if len(linux_keyboards) > 0 {
		return true
	}
	if grab && !linux_output_init() {
		return false
	}

	linux_keyboard_owner = owner
	linux_keyboard_grab_devices = grab
	linux_keyboard_shift_down = false
	linux_keyboard_control_down = false
	linux_keyboard_option_down = false
	linux_keyboard_command_down = false

	if !linux_keyboard_open_devices(grab) {
		fmt.eprintln("stoin: failed to open Linux keyboard devices under /dev/input/event*")
		linux_keyboard_stop()
		return false
	}

	sync.atomic_store(&linux_keyboard_running, true)
	return true
}

linux_keyboard_stop :: proc() {
	sync.atomic_store(&linux_keyboard_running, false)
	linux_keyboard_close_devices()
	linux_keyboard_owner = nil
	linux_keyboard_grab_devices = false
	linux_keyboard_shift_down = false
	linux_keyboard_control_down = false
	linux_keyboard_option_down = false
	linux_keyboard_command_down = false
}

linux_qwerty_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	return linux_keyboard_start(owner, true)
}

linux_qwerty_run :: proc() {
	for sync.atomic_load(&linux_keyboard_running) {
		_ = linux_keyboard_poll_once(250)
	}
}

linux_qwerty_stop :: proc() {
	linux_keyboard_stop()
}

linux_keyboard_listen_signal_ready :: proc(ok: bool) {
	sync.mutex_lock(&linux_keyboard_thread_mutex)
	linux_keyboard_thread_ok = ok
	linux_keyboard_thread_ready = true
	sync.cond_signal(&linux_keyboard_thread_condition)
	sync.mutex_unlock(&linux_keyboard_thread_mutex)
}

linux_keyboard_listen_thread_main :: proc(data: rawptr) {
	context = runtime.default_context()
	owner := (^Steno_Runtime_Owner)(data)
	ok := linux_keyboard_start(owner, false)
	linux_keyboard_listen_signal_ready(ok)
	if ok {
		for sync.atomic_load(&linux_keyboard_running) {
			_ = linux_keyboard_poll_once(250)
		}
		linux_keyboard_stop()
	}
}

linux_keyboard_listen_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if linux_keyboard_thread != nil {
		return true
	}

	sync.mutex_lock(&linux_keyboard_thread_mutex)
	linux_keyboard_thread_ready = false
	linux_keyboard_thread_ok = false
	sync.mutex_unlock(&linux_keyboard_thread_mutex)

	linux_keyboard_thread = thread.create_and_start_with_data(rawptr(owner), linux_keyboard_listen_thread_main)
	if linux_keyboard_thread == nil {
		return false
	}

	sync.mutex_lock(&linux_keyboard_thread_mutex)
	for !linux_keyboard_thread_ready {
		sync.cond_wait(&linux_keyboard_thread_condition, &linux_keyboard_thread_mutex)
	}
	ok := linux_keyboard_thread_ok
	sync.mutex_unlock(&linux_keyboard_thread_mutex)

	if !ok {
		thread.destroy(linux_keyboard_thread)
		linux_keyboard_thread = nil
		return false
	}
	return true
}

linux_keyboard_listen_stop :: proc() {
	if linux_keyboard_thread != nil {
		sync.atomic_store(&linux_keyboard_running, false)
		thread.destroy(linux_keyboard_thread)
		linux_keyboard_thread = nil
		return
	}
	linux_keyboard_stop()
}
