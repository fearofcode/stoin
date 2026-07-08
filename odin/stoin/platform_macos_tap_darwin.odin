package stoin

import "base:runtime"
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
		return nil
	}
	return event
}

macos_qwerty_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if macos_tap != nil {
		return true
	}
	if !macos_output_init() {
		return false
	}

	events := macos_event_mask_bit(KCG_EVENT_KEY_DOWN) |
		macos_event_mask_bit(KCG_EVENT_KEY_UP) |
		macos_event_mask_bit(KCG_EVENT_FLAGS_CHANGED)
	macos_tap = CGEventTapCreate(
		KCG_SESSION_EVENT_TAP,
		KCG_HEAD_INSERT_EVENT_TAP,
		KCG_EVENT_TAP_OPTION_DEFAULT,
		events,
		macos_keyboard_tap_callback,
		rawptr(owner),
	)
	if macos_tap == nil {
		return false
	}

	macos_run_loop_source = CFMachPortCreateRunLoopSource(nil, macos_tap, 0)
	if macos_run_loop_source == nil {
		macos_qwerty_stop()
		return false
	}
	macos_run_loop = CFRunLoopGetCurrent()
	CFRunLoopAddSource(macos_run_loop, macos_run_loop_source, macos_run_loop_mode())
	CGEventTapEnable(macos_tap, true)
	return true
}

macos_qwerty_run :: proc() {
	CFRunLoopRun()
}

macos_qwerty_stop :: proc() {
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
}
