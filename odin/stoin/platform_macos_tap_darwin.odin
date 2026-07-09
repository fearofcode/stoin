package stoin

import "base:runtime"
import "core:sync"
import "core:thread"
import CF "core:sys/darwin/CoreFoundation"

foreign import CoreFoundation "system:CoreFoundation.framework"

CFRunLoopRef :: distinct CF.TypeRef
CFRunLoopSourceRef :: distinct CF.TypeRef

foreign CoreFoundation {
	CFRunLoopGetCurrent :: proc() -> CFRunLoopRef ---
	CFRunLoopRun :: proc() ---
	CFRunLoopStop :: proc(run_loop: CFRunLoopRef) ---
	CFRunLoopAddSource :: proc(run_loop: CFRunLoopRef, source: CFRunLoopSourceRef, mode: CF.String) ---
	CFRunLoopRemoveSource :: proc(run_loop: CFRunLoopRef, source: CFRunLoopSourceRef, mode: CF.String) ---
	CFRunLoopSourceInvalidate :: proc(source: CFRunLoopSourceRef) ---
	CFMachPortCreateRunLoopSource :: proc(allocator: CF.TypeRef, port: CFMachPortRef, order: CF.Index) -> CFRunLoopSourceRef ---
	CFMachPortInvalidate :: proc(port: CFMachPortRef) ---
}

macos_tap: CFMachPortRef
macos_run_loop_source: CFRunLoopSourceRef
macos_run_loop: CFRunLoopRef
macos_tap_listen_only: bool
macos_tap_thread: ^thread.Thread
macos_tap_thread_mutex: sync.Mutex
macos_tap_thread_condition: sync.Cond
macos_tap_thread_ready: bool
macos_tap_thread_ok: bool

macos_run_loop_mode :: proc() -> CF.String {
	return CF.STR("kCFRunLoopDefaultMode")
}

macos_event_mask_bit :: proc(event_type: CGEventType) -> CGEventMask {
	return CGEventMask(u64(1) << uint(event_type))
}

macos_flags_key_is_down :: proc(keycode: u16, flags: CGEventFlags) -> bool {
	switch keycode {
	case 56, 60:
		return (flags & KCG_EVENT_FLAG_MASK_SHIFT) != 0
	case 58, 61:
		return (flags & KCG_EVENT_FLAG_MASK_ALTERNATE) != 0
	case 59, 62:
		return (flags & KCG_EVENT_FLAG_MASK_CONTROL) != 0
	case 55, 54:
		return (flags & KCG_EVENT_FLAG_MASK_COMMAND) != 0
	}
	return false
}

macos_keyboard_tap_callback :: proc "c" (proxy: CGEventTapProxy, event_type: CGEventType, event: CGEventRef, user_info: rawptr) -> CGEventRef {
	context = runtime.default_context()
	_ = proxy

	if event_type == KCG_EVENT_TAP_DISABLED_BY_TIMEOUT {
		if macos_tap != nil {
			CGEventTapEnable(macos_tap, true)
		}
		return nil
	}

	if event_type != KCG_EVENT_KEY_DOWN &&
	   event_type != KCG_EVENT_KEY_UP &&
	   event_type != KCG_EVENT_FLAGS_CHANGED {
		return event
	}
	if macos_event_was_generated_by_stoin(event) {
		return event
	}

	flags := CGEventGetFlags(event)
	keycode := u16(CGEventGetIntegerValueField(event, KCG_KEYBOARD_EVENT_KEYCODE))
	repeat := CGEventGetIntegerValueField(event, KCG_KEYBOARD_EVENT_AUTOREPEAT)
	is_down := event_type == KCG_EVENT_KEY_DOWN
	if event_type == KCG_EVENT_FLAGS_CHANGED {
		is_down = macos_flags_key_is_down(keycode, flags)
	}

	input := Input_Event {
		keycode = keycode,
		is_down = is_down,
		is_repeat = repeat != 0,
		control = (flags & KCG_EVENT_FLAG_MASK_CONTROL) != 0,
		option = (flags & KCG_EVENT_FLAG_MASK_ALTERNATE) != 0,
		command = (flags & KCG_EVENT_FLAG_MASK_COMMAND) != 0,
	}

	owner := (^Steno_Runtime_Owner)(user_info)
	if owner != nil && steno_runtime_owner_handle_event(owner, input) {
		if !macos_tap_listen_only {
			return nil
		}
	}
	return event
}

macos_keyboard_tap_clear :: proc() {
	if macos_tap != nil {
		CGEventTapEnable(macos_tap, false)
	}
	if macos_run_loop != nil && macos_run_loop_source != nil {
		CFRunLoopRemoveSource(macos_run_loop, macos_run_loop_source, macos_run_loop_mode())
	}
	if macos_run_loop_source != nil {
		CFRunLoopSourceInvalidate(macos_run_loop_source)
		CF.Release(CF.TypeRef(macos_run_loop_source))
		macos_run_loop_source = nil
	}
	if macos_tap != nil {
		CFMachPortInvalidate(macos_tap)
		CF.Release(CF.TypeRef(macos_tap))
		macos_tap = nil
	}
	macos_run_loop = nil
	macos_tap_listen_only = false
}

macos_keyboard_tap_start :: proc(owner: ^Steno_Runtime_Owner, listen_only: bool) -> bool {
	if owner == nil {
		return false
	}
	if macos_tap != nil {
		return true
	}
	if !listen_only && !macos_output_init() {
		return false
	}
	macos_tap_listen_only = listen_only

	events := macos_event_mask_bit(KCG_EVENT_KEY_DOWN) |
		macos_event_mask_bit(KCG_EVENT_KEY_UP) |
		macos_event_mask_bit(KCG_EVENT_FLAGS_CHANGED)
	tap_option := KCG_EVENT_TAP_OPTION_DEFAULT
	if listen_only {
		tap_option = KCG_EVENT_TAP_OPTION_LISTEN_ONLY
	}
	macos_tap = CGEventTapCreate(
		KCG_SESSION_EVENT_TAP,
		KCG_HEAD_INSERT_EVENT_TAP,
		tap_option,
		events,
		macos_keyboard_tap_callback,
		rawptr(owner),
	)
	if macos_tap == nil {
		macos_tap_listen_only = false
		return false
	}

	macos_run_loop_source = CFMachPortCreateRunLoopSource(nil, macos_tap, 0)
	if macos_run_loop_source == nil {
		macos_keyboard_tap_clear()
		return false
	}
	macos_run_loop = CFRunLoopGetCurrent()
	CFRunLoopAddSource(macos_run_loop, macos_run_loop_source, macos_run_loop_mode())
	CGEventTapEnable(macos_tap, true)
	return true
}

macos_qwerty_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	return macos_keyboard_tap_start(owner, false)
}

macos_qwerty_run :: proc() {
	CFRunLoopRun()
}

macos_keyboard_listen_signal_ready :: proc(ok: bool) {
	sync.mutex_lock(&macos_tap_thread_mutex)
	macos_tap_thread_ok = ok
	macos_tap_thread_ready = true
	sync.cond_signal(&macos_tap_thread_condition)
	sync.mutex_unlock(&macos_tap_thread_mutex)
}

macos_keyboard_listen_thread_main :: proc(data: rawptr) {
	context = runtime.default_context()
	owner := (^Steno_Runtime_Owner)(data)
	ok := macos_keyboard_tap_start(owner, true)
	macos_keyboard_listen_signal_ready(ok)
	if ok {
		CFRunLoopRun()
		macos_keyboard_tap_clear()
	}
}

macos_keyboard_listen_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if macos_tap_thread != nil {
		return true
	}

	sync.mutex_lock(&macos_tap_thread_mutex)
	macos_tap_thread_ready = false
	macos_tap_thread_ok = false
	sync.mutex_unlock(&macos_tap_thread_mutex)

	macos_tap_thread = thread.create_and_start_with_data(rawptr(owner), macos_keyboard_listen_thread_main)
	if macos_tap_thread == nil {
		return false
	}

	sync.mutex_lock(&macos_tap_thread_mutex)
	for !macos_tap_thread_ready {
		sync.cond_wait(&macos_tap_thread_condition, &macos_tap_thread_mutex)
	}
	ok := macos_tap_thread_ok
	sync.mutex_unlock(&macos_tap_thread_mutex)

	if !ok {
		thread.destroy(macos_tap_thread)
		macos_tap_thread = nil
		return false
	}
	return true
}

macos_keyboard_tap_stop :: proc() {
	if macos_tap_thread != nil {
		if macos_run_loop != nil {
			CFRunLoopStop(macos_run_loop)
		}
		thread.destroy(macos_tap_thread)
		macos_tap_thread = nil
		return
	}
	macos_keyboard_tap_clear()
}

macos_keyboard_listen_stop :: proc() {
	macos_keyboard_tap_stop()
}

macos_qwerty_stop :: proc() {
	macos_keyboard_tap_stop()
}
