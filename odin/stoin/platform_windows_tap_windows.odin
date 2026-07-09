#+build windows
package stoin

import "base:runtime"
import "core:c"
import "core:sync"
import win "core:sys/windows"
import "core:thread"

WINDOWS_LLKHF_EXTENDED :: win.DWORD(0x01)

windows_keyboard_hook: win.HHOOK
windows_keyboard_owner: ^Steno_Runtime_Owner
windows_keyboard_thread_id: win.DWORD
windows_keyboard_down_vks: [256]bool
windows_keyboard_shift_down: bool
windows_keyboard_control_down: bool
windows_keyboard_option_down: bool
windows_keyboard_command_down: bool
windows_keyboard_running: bool
windows_keyboard_thread: ^thread.Thread
windows_keyboard_thread_mutex: sync.Mutex
windows_keyboard_thread_condition: sync.Cond
windows_keyboard_thread_ready: bool
windows_keyboard_thread_ok: bool

windows_logical_keycode_from_vk :: proc(vk: win.UINT) -> (u16, bool) {
	switch vk {
	case win.VK_A: return 0, true
	case win.VK_S: return 1, true
	case win.VK_D: return 2, true
	case win.VK_F: return 3, true
	case win.VK_H: return 4, true
	case win.VK_G: return 5, true
	case win.VK_Z: return 6, true
	case win.VK_X: return 7, true
	case win.VK_C: return 8, true
	case win.VK_V: return 9, true
	case win.VK_B: return 11, true
	case win.VK_Q: return 12, true
	case win.VK_W: return 13, true
	case win.VK_E: return 14, true
	case win.VK_R: return 15, true
	case win.VK_Y: return 16, true
	case win.VK_T: return 17, true
	case win.VK_1: return 18, true
	case win.VK_2: return 19, true
	case win.VK_3: return 20, true
	case win.VK_4: return 21, true
	case win.VK_6: return 22, true
	case win.VK_5: return 23, true
	case win.VK_9: return 25, true
	case win.VK_7: return 26, true
	case win.VK_8: return 28, true
	case win.VK_0: return 29, true
	case win.VK_OEM_6: return 30, true
	case win.VK_O: return 31, true
	case win.VK_U: return 32, true
	case win.VK_OEM_4: return 33, true
	case win.VK_I: return 34, true
	case win.VK_P: return 35, true
	case win.VK_RETURN: return 36, true
	case win.VK_L: return 37, true
	case win.VK_J: return 38, true
	case win.VK_OEM_7: return 39, true
	case win.VK_K: return 40, true
	case win.VK_OEM_1: return 41, true
	case win.VK_OEM_5: return 42, true
	case win.VK_OEM_COMMA: return 43, true
	case win.VK_OEM_2: return 44, true
	case win.VK_N: return 45, true
	case win.VK_M: return 46, true
	case win.VK_OEM_PERIOD: return 47, true
	case win.VK_TAB: return 48, true
	case win.VK_SPACE: return 49, true
	case win.VK_OEM_3: return 50, true
	case win.VK_BACK: return 51, true
	case win.VK_ESCAPE: return 53, true
	case win.VK_RWIN: return 54, true
	case win.VK_LWIN: return 55, true
	case win.VK_LSHIFT: return 56, true
	case win.VK_LMENU: return 58, true
	case win.VK_LCONTROL: return 59, true
	case win.VK_RSHIFT: return 60, true
	case win.VK_RMENU: return 61, true
	case win.VK_RCONTROL: return 62, true
	case win.VK_F17: return 64, true
	case win.VK_F18: return 79, true
	case win.VK_F19: return 80, true
	case win.VK_F20: return 90, true
	case win.VK_F13: return 105, true
	case win.VK_F16: return 106, true
	case win.VK_F14: return 107, true
	case win.VK_F15: return 113, true
	}
	return 0, false
}

windows_printable_from_logical :: proc(logical: u16) -> byte {
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

windows_update_modifier_state :: proc(logical: u16, is_down: bool) {
	switch logical {
	case KEYCODE_LEFT_COMMAND, KEYCODE_RIGHT_COMMAND:
		windows_keyboard_command_down = is_down
	case 56, 60:
		windows_keyboard_shift_down = is_down
	case KEYCODE_LEFT_OPTION, KEYCODE_RIGHT_OPTION:
		windows_keyboard_option_down = is_down
	case KEYCODE_LEFT_CONTROL, KEYCODE_RIGHT_CONTROL:
		windows_keyboard_control_down = is_down
	}
}

windows_normalize_hook_vk :: proc(event: ^win.KBDLLHOOKSTRUCT) -> win.UINT {
	if event == nil {
		return 0
	}
	switch event.vkCode {
	case win.VK_SHIFT, win.VK_CONTROL, win.VK_MENU:
		scan_code := event.scanCode
		if (event.flags & WINDOWS_LLKHF_EXTENDED) != 0 {
			scan_code |= 0xe000
		}
		return win.MapVirtualKeyW(scan_code, win.MAPVK_VSC_TO_VK_EX)
	}
	return win.UINT(event.vkCode)
}

windows_keyboard_hook_callback :: proc "system" (code: c.int, wparam: win.WPARAM, lparam: win.LPARAM) -> win.LRESULT {
	context = runtime.default_context()
	if code < 0 || lparam == 0 {
		return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
	}

	event := (^win.KBDLLHOOKSTRUCT)(uintptr(lparam))
	if event == nil || event.dwExtraInfo == WINDOWS_STOIN_EXTRA_INFO {
		return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
	}

	is_down := wparam == win.WPARAM(win.WM_KEYDOWN) || wparam == win.WPARAM(win.WM_SYSKEYDOWN)
	is_up := wparam == win.WPARAM(win.WM_KEYUP) || wparam == win.WPARAM(win.WM_SYSKEYUP)
	if !is_down && !is_up {
		return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
	}

	vk := windows_normalize_hook_vk(event)
	if vk >= win.UINT(len(windows_keyboard_down_vks)) {
		return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
	}

	logical, mapped := windows_logical_keycode_from_vk(vk)
	if !mapped {
		return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
	}

	vk_index := int(vk)
	repeat := is_down && windows_keyboard_down_vks[vk_index]
	windows_keyboard_down_vks[vk_index] = is_down
	if !repeat {
		windows_update_modifier_state(logical, is_down)
	}

	consumed := false
	if windows_keyboard_owner != nil {
		input := Input_Event {
			keycode = logical,
			is_down = is_down,
			is_repeat = repeat,
			shift = windows_keyboard_shift_down,
			control = windows_keyboard_control_down,
			option = windows_keyboard_option_down,
			command = windows_keyboard_command_down,
			printable = windows_printable_from_logical(logical),
		}
		consumed = steno_runtime_owner_handle_event(windows_keyboard_owner, input)
	}
	if consumed {
		return 1
	}
	return win.CallNextHookEx(windows_keyboard_hook, code, wparam, lparam)
}

windows_keyboard_clear_state :: proc() {
	windows_keyboard_down_vks = {}
	windows_keyboard_owner = nil
	windows_keyboard_shift_down = false
	windows_keyboard_control_down = false
	windows_keyboard_option_down = false
	windows_keyboard_command_down = false
}

windows_keyboard_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if windows_keyboard_hook != nil {
		return true
	}
	if !windows_output_init() {
		return false
	}

	windows_keyboard_owner = owner
	windows_keyboard_thread_id = win.GetCurrentThreadId()
	module := win.GetModuleHandleW(nil)
	windows_keyboard_hook = win.SetWindowsHookExW(
		win.WH_KEYBOARD_LL,
		windows_keyboard_hook_callback,
		win.HINSTANCE(module),
		0,
	)
	if windows_keyboard_hook == nil {
		windows_keyboard_clear_state()
		return false
	}

	sync.atomic_store(&windows_keyboard_running, true)
	return true
}

windows_keyboard_stop :: proc() {
	sync.atomic_store(&windows_keyboard_running, false)
	if windows_keyboard_hook != nil {
		_ = win.UnhookWindowsHookEx(windows_keyboard_hook)
		windows_keyboard_hook = nil
	}
	if windows_keyboard_thread_id != 0 {
		_ = win.PostThreadMessageW(windows_keyboard_thread_id, win.WM_QUIT, 0, 0)
		windows_keyboard_thread_id = 0
	}
	windows_keyboard_clear_state()
}

windows_qwerty_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	return windows_keyboard_start(owner)
}

windows_qwerty_run :: proc() {
	msg: win.MSG
	for sync.atomic_load(&windows_keyboard_running) {
		result := win.GetMessageW(&msg, nil, 0, 0)
		if result <= 0 {
			break
		}
		_ = win.TranslateMessage(&msg)
		_ = win.DispatchMessageW(&msg)
	}
}

windows_qwerty_stop :: proc() {
	windows_keyboard_stop()
}

windows_keyboard_listen_signal_ready :: proc(ok: bool) {
	sync.mutex_lock(&windows_keyboard_thread_mutex)
	windows_keyboard_thread_ok = ok
	windows_keyboard_thread_ready = true
	sync.cond_signal(&windows_keyboard_thread_condition)
	sync.mutex_unlock(&windows_keyboard_thread_mutex)
}

windows_keyboard_listen_thread_main :: proc(data: rawptr) {
	context = runtime.default_context()
	owner := (^Steno_Runtime_Owner)(data)
	ok := windows_keyboard_start(owner)
	windows_keyboard_listen_signal_ready(ok)
	if ok {
		windows_qwerty_run()
		windows_keyboard_stop()
	}
}

windows_keyboard_listen_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if windows_keyboard_thread != nil {
		return true
	}

	sync.mutex_lock(&windows_keyboard_thread_mutex)
	windows_keyboard_thread_ready = false
	windows_keyboard_thread_ok = false
	sync.mutex_unlock(&windows_keyboard_thread_mutex)

	windows_keyboard_thread = thread.create_and_start_with_data(rawptr(owner), windows_keyboard_listen_thread_main)
	if windows_keyboard_thread == nil {
		return false
	}

	sync.mutex_lock(&windows_keyboard_thread_mutex)
	for !windows_keyboard_thread_ready {
		sync.cond_wait(&windows_keyboard_thread_condition, &windows_keyboard_thread_mutex)
	}
	ok := windows_keyboard_thread_ok
	sync.mutex_unlock(&windows_keyboard_thread_mutex)

	if !ok {
		thread.destroy(windows_keyboard_thread)
		windows_keyboard_thread = nil
		return false
	}
	return true
}

windows_keyboard_listen_stop :: proc() {
	if windows_keyboard_thread != nil {
		windows_keyboard_stop()
		thread.destroy(windows_keyboard_thread)
		windows_keyboard_thread = nil
		return
	}
	windows_keyboard_stop()
}
