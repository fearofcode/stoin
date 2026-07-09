package stoin

import "core:fmt"
import "core:os"
import "core:sync"
import "core:time"

TRANSLATION_COMPACT_INTERVAL_STROKES :: 1000
TRANSLATION_HISTORY_STROKE_LIMIT :: 1000

Send_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Delete_Text_Callback :: proc(text: string, userdata: rawptr) -> bool
Send_Key_Combination_Callback :: proc(combo: string, userdata: rawptr) -> bool
Line_Output_Callback :: proc(line: string, userdata: rawptr) -> bool
Translation_Timing_Begin_Callback :: proc(start_ns: u64, userdata: rawptr)
Translation_Timing_Cancel_Callback :: proc(userdata: rawptr)

File_Stamp :: struct {
	exists:           bool,
	size:             i64,
	modified_time_ns: i64,
}

Steno_Phrase_Mode :: enum {
	None,
	All,
	Verbs,
	Nonverbs,
}

Stroke_Input :: struct {
	bits:             u64,
	received_ns:      u64,
	phrase:           bool,
	phrase_namespace: bool,
	phrase_mode:      Steno_Phrase_Mode,
}

Steno_Runtime_Config :: struct {
	dictionary:           ^Dictionary,
	dictionary_stack:     ^Dictionary_Stack,
	orthography:          ^Orthography,
	phrasing:             ^Phrasing,
	keymap:               ^Keymap,
	send_text:            Send_Text_Callback,
	delete_text:          Delete_Text_Callback,
	send_key_combination: Send_Key_Combination_Callback,
	write_trace:          Line_Output_Callback,
	write_suggestion:     Line_Output_Callback,
	write_suggestion_log: Line_Output_Callback,
	begin_translation_timing: Translation_Timing_Begin_Callback,
	cancel_translation_timing: Translation_Timing_Cancel_Callback,
	userdata:             rawptr,
}

Steno_Runtime_Load_Config :: struct {
	dictionary_paths:     []string,
	dictionary_enabled:   []bool,
	keymap_path:          string,
	orthography_path:     string,
	phrasing_path:        string,
	send_text:            Send_Text_Callback,
	delete_text:          Delete_Text_Callback,
	send_key_combination: Send_Key_Combination_Callback,
	write_trace:          Line_Output_Callback,
	write_suggestion:     Line_Output_Callback,
	write_suggestion_log: Line_Output_Callback,
	begin_translation_timing: Translation_Timing_Begin_Callback,
	cancel_translation_timing: Translation_Timing_Cancel_Callback,
	userdata:             rawptr,
}

Steno_Runtime :: struct {
	engine:                   Simple_Engine,
	phrasing:                 ^Phrasing,
	keymap:                   ^Keymap,
	send_text:                Send_Text_Callback,
	delete_text:              Delete_Text_Callback,
	send_key_combination:     Send_Key_Combination_Callback,
	write_trace:              Line_Output_Callback,
	write_suggestion:         Line_Output_Callback,
	write_suggestion_log:     Line_Output_Callback,
	begin_translation_timing: Translation_Timing_Begin_Callback,
	cancel_translation_timing: Translation_Timing_Cancel_Callback,
	userdata:                 rawptr,
	session_active:           bool,
	trace_key_events:         bool,
	phrase_namespace_enabled: bool,
	enabled:                  bool,
	phrase_mode:              Steno_Phrase_Mode,
	phrase_toggle_enabled:    bool,
	phrase_toggle_keycode:    u16,
	phrase_toggle_down:       bool,
	phrase_toggle_latched:    bool,
	nonverb_phrase_toggle_enabled: bool,
	nonverb_phrase_toggle_keycode: u16,
	nonverb_phrase_toggle_down:    bool,
	nonverb_phrase_toggle_latched: bool,
	strokes_since_compaction: int,
	down_keycodes:            u64,
	chord_bits:               u64,
	chord_phrase_mode:        Steno_Phrase_Mode,
	toggle_esc_down:          bool,
	command_down:             bool,
	option_down:              bool,
	control_down:             bool,
}

Steno_Runtime_Owner :: struct {
	dictionary_stack: Dictionary_Stack,
	keymap:           Keymap,
	has_keymap:       bool,
	orthography:      Orthography,
	has_orthography:  bool,
	phrasing:         Phrasing,
	has_phrasing:     bool,
	phrasing_path:    string,
	dictionary_stamps: [dynamic]File_Stamp,
	dictionary_reload_error_reported: bool,
	phrasing_stamp:   File_Stamp,
	phrasing_stamp_valid: bool,
	phrasing_reload_error_reported: bool,
	runtime:          Steno_Runtime,
}

Input_Event :: struct {
	keycode:   u16,
	is_down:   bool,
	is_repeat: bool,
	shift:     bool,
	command:   bool,
	option:    bool,
	control:   bool,
}

KEYCODE_ESCAPE :: u16(53)
KEYCODE_LEFT_COMMAND :: u16(55)
KEYCODE_RIGHT_COMMAND :: u16(54)
KEYCODE_LEFT_OPTION :: u16(58)
KEYCODE_RIGHT_OPTION :: u16(61)
KEYCODE_LEFT_CONTROL :: u16(59)
KEYCODE_RIGHT_CONTROL :: u16(62)

Trace_Stroke_Mode :: enum {
	Normal,
	Phrase,
	Phase_Fallback,
}

steno_runtime_init :: proc(runtime: ^Steno_Runtime, config: ^Steno_Runtime_Config) -> bool {
	if runtime == nil || config == nil || config.send_text == nil || config.delete_text == nil {
		return false
	}
	if config.dictionary_stack != nil {
		simple_engine_init_with_stack(&runtime.engine, config.dictionary_stack)
	} else if config.dictionary != nil {
		simple_engine_init(&runtime.engine, config.dictionary)
	} else {
		return false
	}

	if config.orthography != nil {
		simple_engine_set_orthography(&runtime.engine, config.orthography)
	}
	runtime.phrasing = config.phrasing
	runtime.keymap = config.keymap
	runtime.send_text = config.send_text
	runtime.delete_text = config.delete_text
	runtime.send_key_combination = config.send_key_combination
	runtime.write_trace = config.write_trace
	runtime.write_suggestion = config.write_suggestion
	runtime.write_suggestion_log = config.write_suggestion_log
	runtime.begin_translation_timing = config.begin_translation_timing
	runtime.cancel_translation_timing = config.cancel_translation_timing
	runtime.userdata = config.userdata
	runtime.enabled = true
	runtime.session_active = true
	return true
}

steno_runtime_owner_init :: proc(owner: ^Steno_Runtime_Owner, config: ^Steno_Runtime_Load_Config) -> bool {
	if owner == nil || config == nil {
		return false
	}
	owner^ = {}

	dictionary_stack_init(&owner.dictionary_stack)
	if !dictionary_stack_set_paths(&owner.dictionary_stack, config.dictionary_paths, config.dictionary_enabled) ||
	   !dictionary_stack_load(&owner.dictionary_stack) {
		steno_runtime_owner_destroy(owner)
		return false
	}

	keymap_pointer: ^Keymap
	if len(config.keymap_path) > 0 {
		keymap_init(&owner.keymap)
		owner.has_keymap = true
		if !keymap_load(&owner.keymap, config.keymap_path) {
			steno_runtime_owner_destroy(owner)
			return false
		}
		keymap_pointer = &owner.keymap
	}

	orthography_pointer: ^Orthography
	if len(config.orthography_path) > 0 {
		orthography_init(&owner.orthography)
		owner.has_orthography = true
		if !orthography_load(&owner.orthography, config.orthography_path) {
			steno_runtime_owner_destroy(owner)
			return false
		}
		orthography_pointer = &owner.orthography
	}

	phrasing_pointer: ^Phrasing
	if len(config.phrasing_path) > 0 {
		phrasing_ok: bool
		owner.phrasing, phrasing_ok = phrasing_load(config.phrasing_path)
		if !phrasing_ok {
			steno_runtime_owner_destroy(owner)
			return false
		}
		owner.has_phrasing = true
		path_copy, path_ok := clone_string_ok(config.phrasing_path)
		if !path_ok {
			steno_runtime_owner_destroy(owner)
			return false
		}
		owner.phrasing_path = path_copy
		phrasing_pointer = &owner.phrasing
	}

	runtime_config := Steno_Runtime_Config {
		dictionary_stack = &owner.dictionary_stack,
		keymap = keymap_pointer,
		orthography = orthography_pointer,
		phrasing = phrasing_pointer,
		send_text = config.send_text,
		delete_text = config.delete_text,
		send_key_combination = config.send_key_combination,
		write_trace = config.write_trace,
		write_suggestion = config.write_suggestion,
		write_suggestion_log = config.write_suggestion_log,
		begin_translation_timing = config.begin_translation_timing,
		cancel_translation_timing = config.cancel_translation_timing,
		userdata = config.userdata,
	}
	if !steno_runtime_init(&owner.runtime, &runtime_config) {
		steno_runtime_owner_destroy(owner)
		return false
	}
	_ = steno_runtime_owner_refresh_dictionary_stamps(owner)
	_ = steno_runtime_owner_refresh_phrasing_stamp(owner)

	return true
}

steno_runtime_owner_destroy :: proc(owner: ^Steno_Runtime_Owner) {
	if owner == nil {
		return
	}
	steno_runtime_destroy(&owner.runtime)
	if owner.has_phrasing {
		phrasing_destroy(&owner.phrasing)
	}
	owned_string_delete(owner.phrasing_path)
	delete(owner.dictionary_stamps)
	if owner.has_orthography {
		orthography_destroy(&owner.orthography)
	}
	if owner.has_keymap {
		keymap_destroy(&owner.keymap)
	}
	dictionary_stack_destroy(&owner.dictionary_stack)
	owner^ = {}
}

file_stamp_read :: proc(path: string) -> (stamp: File_Stamp, ok: bool) {
	if len(path) == 0 {
		return {}, false
	}

	info, stat_err := os.stat(path, context.allocator)
	if stat_err != nil {
		if stat_err == .Not_Exist {
			return {}, true
		}
		return {}, false
	}
	defer os.file_info_delete(info, context.allocator)

	return File_Stamp {
		exists = true,
		size = info.size,
		modified_time_ns = time.time_to_unix_nano(info.modification_time),
	}, true
}

file_stamps_equal :: proc(a: File_Stamp, b: File_Stamp) -> bool {
	return a.exists == b.exists &&
		a.size == b.size &&
		a.modified_time_ns == b.modified_time_ns
}

steno_runtime_owner_refresh_dictionary_stamps :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}

	clear(&owner.dictionary_stamps)
	for path in owner.dictionary_stack.paths {
		stamp, ok := file_stamp_read(path)
		if !ok {
			clear(&owner.dictionary_stamps)
			return false
		}
		append(&owner.dictionary_stamps, stamp)
	}
	return true
}

steno_runtime_owner_dictionary_files_changed :: proc(owner: ^Steno_Runtime_Owner) -> (changed: bool, ok: bool) {
	if owner == nil {
		return false, false
	}
	if len(owner.dictionary_stamps) != len(owner.dictionary_stack.paths) {
		return true, true
	}

	for path, i in owner.dictionary_stack.paths {
		stamp, stamp_ok := file_stamp_read(path)
		if !stamp_ok {
			return false, false
		}
		if !file_stamps_equal(stamp, owner.dictionary_stamps[i]) {
			return true, true
		}
	}

	return false, true
}

steno_runtime_owner_reload_dictionary :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if !dictionary_stack_load(&owner.dictionary_stack) {
		_ = steno_runtime_owner_refresh_dictionary_stamps(owner)
		if !owner.dictionary_reload_error_reported {
			fmt.eprintln("stoin: dictionary changed but reload failed; keeping previous dictionary")
			owner.dictionary_reload_error_reported = true
		}
		return false
	}
	if !steno_runtime_owner_refresh_dictionary_stamps(owner) {
		fmt.eprintln("stoin: warning: reloaded dictionary, but failed to refresh dictionary file stamps")
	}
	owner.dictionary_reload_error_reported = false
	fmt.eprintln("stoin: reloaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
	return true
}

steno_runtime_owner_reload_dictionary_if_changed :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	changed, ok := steno_runtime_owner_dictionary_files_changed(owner)
	if !ok {
		if !owner.dictionary_reload_error_reported {
			fmt.eprintln("stoin: failed to check dictionary files for changes")
			owner.dictionary_reload_error_reported = true
		}
		return false
	}
	if !changed {
		return true
	}
	owner.dictionary_reload_error_reported = false
	return steno_runtime_owner_reload_dictionary(owner)
}

steno_runtime_owner_refresh_phrasing_stamp :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil || len(owner.phrasing_path) == 0 {
		if owner != nil {
			owner.phrasing_stamp_valid = false
		}
		return true
	}

	stamp, ok := file_stamp_read(owner.phrasing_path)
	if !ok {
		owner.phrasing_stamp_valid = false
		return false
	}

	owner.phrasing_stamp = stamp
	owner.phrasing_stamp_valid = true
	return true
}

steno_runtime_owner_phrasing_file_changed :: proc(owner: ^Steno_Runtime_Owner) -> (changed: bool, ok: bool) {
	if owner == nil {
		return false, false
	}
	if len(owner.phrasing_path) == 0 {
		return false, true
	}
	if !owner.phrasing_stamp_valid {
		return true, true
	}

	stamp, stamp_ok := file_stamp_read(owner.phrasing_path)
	if !stamp_ok {
		owner.phrasing_stamp_valid = false
		return false, false
	}

	return !file_stamps_equal(stamp, owner.phrasing_stamp), true
}

steno_runtime_owner_reload_phrasing :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	if len(owner.phrasing_path) == 0 {
		return true
	}

	next, ok := phrasing_load(owner.phrasing_path)
	if !ok {
		_ = steno_runtime_owner_refresh_phrasing_stamp(owner)
		if !owner.phrasing_reload_error_reported {
			fmt.eprintln("stoin: phrasing changed but reload failed; keeping previous phrasing")
			owner.phrasing_reload_error_reported = true
		}
		return false
	}
	if owner.has_phrasing {
		phrasing_destroy(&owner.phrasing)
	}
	owner.phrasing = next
	owner.has_phrasing = true
	owner.runtime.phrasing = &owner.phrasing
	if !steno_runtime_owner_refresh_phrasing_stamp(owner) {
		fmt.eprintln("stoin: warning: reloaded phrasing, but failed to refresh phrasing file stamp")
	}
	owner.phrasing_reload_error_reported = false
	fmt.eprintln("stoin: reloaded phrasing from", owner.phrasing_path)
	return true
}

steno_runtime_owner_reload_phrasing_if_changed :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	if owner == nil {
		return false
	}
	changed, ok := steno_runtime_owner_phrasing_file_changed(owner)
	if !ok {
		if !owner.phrasing_reload_error_reported {
			fmt.eprintln("stoin: failed to check phrasing file for changes")
			owner.phrasing_reload_error_reported = true
		}
		return false
	}
	if !changed {
		return true
	}
	owner.phrasing_reload_error_reported = false
	return steno_runtime_owner_reload_phrasing(owner)
}

steno_runtime_owner_reload_files_if_changed :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	dictionary_ok := steno_runtime_owner_reload_dictionary_if_changed(owner)
	phrasing_ok := steno_runtime_owner_reload_phrasing_if_changed(owner)
	return dictionary_ok && phrasing_ok
}

steno_runtime_owner_handle_stroke :: proc(owner: ^Steno_Runtime_Owner, stroke: Stroke_Input) -> bool {
	if owner == nil {
		return false
	}
	_ = steno_runtime_owner_reload_files_if_changed(owner)
	return steno_runtime_handle_stroke(&owner.runtime, stroke)
}

steno_runtime_owner_handle_stroke_bits :: proc(owner: ^Steno_Runtime_Owner, bits: u64) -> bool {
	if owner == nil {
		return false
	}
	return steno_runtime_owner_handle_stroke(owner, Stroke_Input{bits = bits})
}

steno_runtime_owner_handle_active_stroke_bits :: proc(owner: ^Steno_Runtime_Owner, bits: u64) -> bool {
	if owner == nil {
		return false
	}
	return steno_runtime_owner_handle_stroke(owner, Stroke_Input {
		bits = bits,
		phrase_namespace = steno_runtime_phrase_namespace_active(&owner.runtime),
		phrase_mode = steno_runtime_current_phrase_mode(&owner.runtime, true),
	})
}

steno_runtime_owner_handle_active_stroke_bits_received :: proc(owner: ^Steno_Runtime_Owner, bits: u64, received_ns: u64) -> bool {
	if owner == nil {
		return false
	}
	return steno_runtime_owner_handle_stroke(owner, Stroke_Input {
		bits = bits,
		received_ns = received_ns,
		phrase_namespace = steno_runtime_phrase_namespace_active(&owner.runtime),
		phrase_mode = steno_runtime_current_phrase_mode(&owner.runtime, true),
	})
}

steno_runtime_owner_handle_event :: proc(owner: ^Steno_Runtime_Owner, event: Input_Event) -> bool {
	if owner == nil {
		return false
	}
	_ = steno_runtime_owner_reload_files_if_changed(owner)
	return steno_runtime_handle_event(&owner.runtime, event)
}

steno_runtime_destroy :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	simple_engine_destroy(&runtime.engine)
	runtime^ = {}
}

steno_runtime_set_session_active :: proc(runtime: ^Steno_Runtime, active: bool) {
	if runtime == nil {
		return
	}
	runtime.session_active = active
	if !active {
		runtime.phrase_mode = .None
		steno_runtime_reset_chord(runtime)
	}
}

steno_runtime_set_phrase_namespace_enabled :: proc(runtime: ^Steno_Runtime, enabled: bool) {
	if runtime == nil {
		return
	}
	runtime.phrase_namespace_enabled = enabled
	if !enabled {
		runtime.phrase_mode = .None
		runtime.chord_phrase_mode = .None
	}
}

steno_runtime_set_phrase_mode :: proc(runtime: ^Steno_Runtime, mode: Steno_Phrase_Mode) {
	if runtime == nil {
		return
	}
	runtime.phrase_mode = mode
	if mode != .None && runtime.down_keycodes != 0 {
		runtime.chord_phrase_mode = mode
	}
}

steno_runtime_set_trace_key_events :: proc(runtime: ^Steno_Runtime, enabled: bool) {
	if runtime == nil {
		return
	}
	runtime.trace_key_events = enabled
}

steno_runtime_configure_phrase_toggles :: proc(
	runtime: ^Steno_Runtime,
	phrase_enabled: bool,
	phrase_keycode: u16,
	nonverb_enabled: bool,
	nonverb_keycode: u16,
) {
	if runtime == nil {
		return
	}
	runtime.phrase_toggle_enabled = phrase_enabled
	runtime.phrase_toggle_keycode = phrase_keycode
	runtime.nonverb_phrase_toggle_enabled = nonverb_enabled
	runtime.nonverb_phrase_toggle_keycode = nonverb_keycode
	sync.atomic_store(&runtime.phrase_toggle_down, false)
	sync.atomic_store(&runtime.phrase_toggle_latched, false)
	sync.atomic_store(&runtime.nonverb_phrase_toggle_down, false)
	sync.atomic_store(&runtime.nonverb_phrase_toggle_latched, false)
	runtime.chord_phrase_mode = .None
}

steno_runtime_phrase_toggles_enabled :: proc(runtime: ^Steno_Runtime) -> bool {
	return runtime != nil && (runtime.phrase_toggle_enabled || runtime.nonverb_phrase_toggle_enabled)
}

steno_runtime_phrase_namespace_active :: proc(runtime: ^Steno_Runtime) -> bool {
	return runtime != nil && (runtime.phrase_namespace_enabled || steno_runtime_phrase_toggles_enabled(runtime))
}

steno_runtime_toggle_active :: proc(down: ^bool, latched: ^bool, consume_latch: bool) -> bool {
	is_down := sync.atomic_load(down)
	was_latched: bool
	if consume_latch {
		was_latched = sync.atomic_exchange(latched, false)
	} else {
		was_latched = sync.atomic_load(latched)
	}
	return is_down || was_latched
}

steno_runtime_phrase_mode_from_active :: proc(runtime: ^Steno_Runtime, phrase_active: bool, nonverb_active: bool) -> Steno_Phrase_Mode {
	if phrase_active && nonverb_active {
		return .All
	}
	if phrase_active {
		if runtime != nil && runtime.nonverb_phrase_toggle_enabled {
			return .Verbs
		}
		return .All
	}
	if nonverb_active {
		return .Nonverbs
	}
	return .None
}

steno_runtime_current_toggle_phrase_mode :: proc(runtime: ^Steno_Runtime, consume_latches: bool) -> Steno_Phrase_Mode {
	if runtime == nil {
		return .None
	}
	phrase_active := runtime.phrase_toggle_enabled &&
		steno_runtime_toggle_active(&runtime.phrase_toggle_down, &runtime.phrase_toggle_latched, consume_latches)
	nonverb_active := runtime.nonverb_phrase_toggle_enabled &&
		steno_runtime_toggle_active(
			&runtime.nonverb_phrase_toggle_down,
			&runtime.nonverb_phrase_toggle_latched,
			consume_latches,
		)
	return steno_runtime_phrase_mode_from_active(runtime, phrase_active, nonverb_active)
}

steno_runtime_current_phrase_mode :: proc(runtime: ^Steno_Runtime, consume_latches: bool) -> Steno_Phrase_Mode {
	toggle_mode := steno_runtime_current_toggle_phrase_mode(runtime, consume_latches)
	if toggle_mode != .None {
		return toggle_mode
	}
	if runtime == nil {
		return .None
	}
	return runtime.phrase_mode
}

steno_runtime_current_phrase_down_mode :: proc(runtime: ^Steno_Runtime) -> Steno_Phrase_Mode {
	if runtime == nil {
		return .None
	}
	phrase_active := runtime.phrase_toggle_enabled && sync.atomic_load(&runtime.phrase_toggle_down)
	nonverb_active := runtime.nonverb_phrase_toggle_enabled && sync.atomic_load(&runtime.nonverb_phrase_toggle_down)
	toggle_mode := steno_runtime_phrase_mode_from_active(runtime, phrase_active, nonverb_active)
	if toggle_mode != .None {
		return toggle_mode
	}
	return runtime.phrase_mode
}

steno_runtime_monotonic_ns :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec)
}

steno_runtime_begin_translation_timing :: proc(runtime: ^Steno_Runtime, start_ns: u64) -> bool {
	if runtime == nil || runtime.begin_translation_timing == nil || start_ns == 0 {
		return false
	}
	runtime.begin_translation_timing(start_ns, runtime.userdata)
	return true
}

steno_runtime_cancel_translation_timing :: proc(runtime: ^Steno_Runtime) {
	if runtime != nil && runtime.cancel_translation_timing != nil {
		runtime.cancel_translation_timing(runtime.userdata)
	}
}

steno_runtime_print_key_event :: proc(event: Input_Event) {
	if event.is_repeat {
		return
	}
	fmt.printf(
		"stoin: key event keycode=%d %s shift=%d control=%d option=%d command=%d\n",
		event.keycode,
		event.is_down ? "down" : "up",
		event.shift ? 1 : 0,
		event.control ? 1 : 0,
		event.option ? 1 : 0,
		event.command ? 1 : 0,
	)
}

steno_runtime_update_phrase_toggle_state :: proc(down: ^bool, latched: ^bool, event: Input_Event) {
	if event.is_repeat {
		return
	}
	sync.atomic_store(down, event.is_down)
	if event.is_down {
		sync.atomic_store(latched, true)
	}
}

steno_runtime_update_phrase_toggle_from_event :: proc(runtime: ^Steno_Runtime, event: Input_Event) -> bool {
	if runtime == nil {
		return false
	}
	handled := false
	if runtime.phrase_toggle_enabled && event.keycode == runtime.phrase_toggle_keycode {
		steno_runtime_update_phrase_toggle_state(&runtime.phrase_toggle_down, &runtime.phrase_toggle_latched, event)
		handled = true
	} else if runtime.nonverb_phrase_toggle_enabled && event.keycode == runtime.nonverb_phrase_toggle_keycode {
		steno_runtime_update_phrase_toggle_state(
			&runtime.nonverb_phrase_toggle_down,
			&runtime.nonverb_phrase_toggle_latched,
			event,
		)
		handled = true
	}
	if handled && runtime.down_keycodes != 0 {
		mode := steno_runtime_current_phrase_down_mode(runtime)
		if mode != .None {
			runtime.chord_phrase_mode = mode
		}
	}
	return handled
}

steno_runtime_consume_phrase_latches :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	_ = sync.atomic_exchange(&runtime.phrase_toggle_latched, false)
	_ = sync.atomic_exchange(&runtime.nonverb_phrase_toggle_latched, false)
}

steno_runtime_reset_chord :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	runtime.down_keycodes = 0
	runtime.chord_bits = 0
	runtime.chord_phrase_mode = .None
}

keycode_physical_bit :: proc(keycode: u16) -> u64 {
	if keycode >= 64 {
		return 0
	}
	return u64(1) << uint(keycode)
}

steno_runtime_update_shortcut_modifier_state :: proc(runtime: ^Steno_Runtime, event: Input_Event) -> bool {
	switch event.keycode {
	case KEYCODE_LEFT_COMMAND, KEYCODE_RIGHT_COMMAND:
		runtime.command_down = event.is_down
		return true
	case KEYCODE_LEFT_OPTION, KEYCODE_RIGHT_OPTION:
		runtime.option_down = event.is_down
		return true
	case KEYCODE_LEFT_CONTROL, KEYCODE_RIGHT_CONTROL:
		runtime.control_down = event.is_down
		return true
	}
	return false
}

steno_runtime_handle_event :: proc(runtime: ^Steno_Runtime, event: Input_Event) -> bool {
	if runtime == nil || !runtime.session_active {
		return false
	}

	if runtime.trace_key_events {
		steno_runtime_print_key_event(event)
	}

	modifier_key_event := steno_runtime_update_shortcut_modifier_state(runtime, event)
	shortcut_modifier_down := event.command || event.control || event.option ||
		runtime.command_down || runtime.control_down || runtime.option_down

	toggle_event := event.keycode == KEYCODE_ESCAPE && (event.control || runtime.control_down || runtime.toggle_esc_down)
	if toggle_event {
		if event.is_down && !runtime.toggle_esc_down {
			runtime.enabled = !runtime.enabled
			steno_runtime_reset_chord(runtime)
		}
		runtime.toggle_esc_down = event.is_down
		return true
	}

	if steno_runtime_update_phrase_toggle_from_event(runtime, event) {
		return true
	}

	if modifier_key_event || !runtime.enabled || shortcut_modifier_down {
		return false
	}
	if runtime.keymap == nil {
		return false
	}

	timing_started := false
	if !event.is_down {
		timing_started = steno_runtime_begin_translation_timing(runtime, steno_runtime_monotonic_ns())
	}
	defer if timing_started {
		steno_runtime_cancel_translation_timing(runtime)
	}

	binding := keymap_find_binding(runtime.keymap, event.keycode)
	if binding == nil || event.keycode >= 64 {
		return false
	}

	physical_bit := keycode_physical_bit(event.keycode)
	if event.is_down {
		if (runtime.down_keycodes & physical_bit) == 0 && !event.is_repeat {
			runtime.down_keycodes |= physical_bit
			runtime.chord_bits |= binding.bits
			mode := steno_runtime_current_phrase_down_mode(runtime)
			if mode != .None {
				runtime.chord_phrase_mode = mode
			}
		}
		return true
	}

	runtime.down_keycodes &= ~physical_bit
	if runtime.down_keycodes == 0 {
		stroke := Stroke_Input {
			bits = runtime.chord_bits,
			phrase_namespace = steno_runtime_phrase_namespace_active(runtime),
			phrase_mode = runtime.chord_phrase_mode,
		}
		steno_runtime_consume_phrase_latches(runtime)
		handled := steno_runtime_handle_stroke(runtime, stroke)
		steno_runtime_reset_chord(runtime)
		return handled
	}
	return true
}

steno_runtime_translation_history_stroke_count :: proc(runtime: ^Steno_Runtime) -> int {
	if runtime == nil {
		return 0
	}
	return simple_engine_history_stroke_count(&runtime.engine)
}

steno_runtime_count_completed_stroke :: proc(runtime: ^Steno_Runtime) {
	if runtime == nil {
		return
	}
	runtime.strokes_since_compaction += 1
	if runtime.strokes_since_compaction < TRANSLATION_COMPACT_INTERVAL_STROKES {
		return
	}

	keep_strokes := TRANSLATION_HISTORY_STROKE_LIMIT
	lookup_strokes := simple_engine_lookup_stroke_limit(runtime.engine.dictionary)
	if keep_strokes < lookup_strokes {
		keep_strokes = lookup_strokes
	}
	simple_engine_compact_history(&runtime.engine, keep_strokes)
	runtime.strokes_since_compaction = 0
}

steno_phrase_lookup_mode_from_runtime_mode :: proc(mode: Steno_Phrase_Mode) -> Phrase_Lookup_Mode {
	switch mode {
	case .Verbs:
		return .Verbs
	case .Nonverbs:
		return .Nonverbs
	case .All, .None:
		return .All
	}
	return .All
}

steno_phrase_mode_from_lookup_mode :: proc(mode: Phrase_Lookup_Mode) -> Steno_Phrase_Mode {
	switch mode {
	case .Verbs:
		return .Verbs
	case .Nonverbs:
		return .Nonverbs
	case .All:
		return .All
	}
	return .All
}

steno_normalize_stroke_phrase_mode :: proc(stroke: Stroke_Input, current_mode: Steno_Phrase_Mode) -> Steno_Phrase_Mode {
	if stroke.phrase_mode != .None {
		return stroke.phrase_mode
	}
	if stroke.phrase {
		return .All
	}
	return current_mode
}

steno_runtime_trace_label :: proc(mode: Trace_Stroke_Mode) -> string {
	switch mode {
	case .Phrase:
		return " [phrase]"
	case .Phase_Fallback:
		return " [phase fallback]"
	case .Normal:
		return ""
	}
	return ""
}

steno_runtime_write_trace :: proc(runtime: ^Steno_Runtime, outline: string, translation: string, has_translation: bool, mode: Trace_Stroke_Mode) -> bool {
	if runtime.write_trace == nil {
		return true
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, outline)
	formatted_append_string(&buffer, steno_runtime_trace_label(mode))
	formatted_append_string(&buffer, " -> ")
	if has_translation {
		formatted_append_string(&buffer, translation)
	} else {
		formatted_append_string(&buffer, "[untranslated]")
	}
	append(&buffer, '\n')

	line, line_ok := clone_bytes_to_string(buffer[:])
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)
	return runtime.write_trace(line, runtime.userdata)
}

steno_runtime_single_stroke_outline :: proc(bits: u64) -> (outline: string, ok: bool) {
	strokes := [?]u64{bits}
	return stroke_sequence_to_string_alloc(strokes[:])
}

steno_runtime_translate_dictionary_bits :: proc(runtime: ^Steno_Runtime, bits: u64, trace_mode: Trace_Stroke_Mode) -> (ok: bool, maybe_suggest: bool) {
	if bits == 0 {
		return true, false
	}

	raw_chord, raw_ok := steno_runtime_single_stroke_outline(bits)
	if !raw_ok {
		return false, false
	}
	defer owned_string_delete(raw_chord)

	if translation, found := dictionary_lookup_bits(runtime.engine.dictionary, bits); found && len(translation) > 0 && translation[0] == '=' {
		if !steno_runtime_write_trace(runtime, raw_chord, translation, true, trace_mode) {
			return false, false
		}
		return simple_engine_execute_command(&runtime.engine, translation, bits), false
	}

	match, match_ok := simple_engine_find_match(&runtime.engine, bits)
	if !match_ok {
		return false, false
	}
	defer translation_match_destroy(&match)

	trace_translation := match.translation
	trace_has_translation := match.found
	if match.suffix_match {
		trace_has_translation = len(match.suffix_base_translation) > 0 && len(match.suffix_translation) > 0
		trace_translation = ""
	}
	if !steno_runtime_write_trace(runtime, match.outline, trace_translation, trace_has_translation, trace_mode) {
		return false, false
	}
	if !simple_engine_apply_match(&runtime.engine, &match) {
		return false, false
	}
	return true, match.found
}

steno_runtime_translate_phrase_namespace_bits :: proc(runtime: ^Steno_Runtime, bits: u64, phrase_mode: Steno_Phrase_Mode) -> (ok: bool, maybe_suggest: bool) {
	if bits == 0 {
		return true, false
	}
	if runtime.phrasing == nil {
		return false, false
	}

	raw_chord, raw_ok := steno_runtime_single_stroke_outline(bits)
	if !raw_ok {
		return false, false
	}
	defer owned_string_delete(raw_chord)

	text, result := phrasing_lookup_mode(
		runtime.phrasing,
		bits,
		steno_phrase_lookup_mode_from_runtime_mode(phrase_mode),
	)
	defer owned_string_delete(text)

	switch result {
	case .Hit:
		if !steno_runtime_write_trace(runtime, raw_chord, text, true, .Phrase) {
			return false, false
		}
		return simple_engine_apply_single_stroke_translation(&runtime.engine, bits, text), false
	case .Error:
		return false, false
	case .Miss:
	}

	if phrase_namespace_should_fallback_to_dictionary(bits) {
		return steno_runtime_translate_dictionary_bits(runtime, bits, .Phase_Fallback)
	}

	if !steno_runtime_write_trace(runtime, raw_chord, "", false, .Phrase) {
		return false, false
	}
	return simple_engine_apply_single_stroke_translation(&runtime.engine, bits, raw_chord), false
}

steno_runtime_translate_stroke :: proc(runtime: ^Steno_Runtime, stroke: Stroke_Input) -> (ok: bool, maybe_suggest: bool) {
	phrase_namespace := stroke.phrase_namespace || steno_runtime_phrase_namespace_active(runtime)
	if !phrase_namespace {
		return steno_runtime_translate_dictionary_bits(runtime, stroke.bits, .Normal)
	}

	phrase_mode := steno_normalize_stroke_phrase_mode(stroke, steno_runtime_current_phrase_mode(runtime, false))
	if phrase_mode == .None {
		return steno_runtime_translate_dictionary_bits(runtime, stroke.bits, .Normal)
	}
	return steno_runtime_translate_phrase_namespace_bits(runtime, stroke.bits, phrase_mode)
}

common_utf8_prefix_bytes :: proc(a: string, b: string) -> int {
	index := 0
	last_boundary := 0
	for index < len(a) && index < len(b) && a[index] == b[index] {
		index += 1
		if index >= len(a) || index >= len(b) || (a[index] & 0xC0) != 0x80 {
			last_boundary = index
		}
	}
	return last_boundary
}

steno_runtime_replace_output_text :: proc(runtime: ^Steno_Runtime, old_text: string, new_text: string) -> bool {
	prefix := common_utf8_prefix_bytes(old_text, new_text)
	delete_suffix := old_text[prefix:]
	insert_suffix := new_text[prefix:]

	if len(delete_suffix) > 0 && !runtime.delete_text(delete_suffix, runtime.userdata) {
		return false
	}
	if len(insert_suffix) > 0 && !runtime.send_text(insert_suffix, runtime.userdata) {
		return false
	}
	return true
}

steno_runtime_send_key_combinations :: proc(runtime: ^Steno_Runtime, first_combo: int) -> bool {
	if first_combo >= len(runtime.engine.key_combos) {
		return true
	}
	if runtime.send_key_combination == nil {
		return false
	}
	for i := first_combo; i < len(runtime.engine.key_combos); i += 1 {
		if !runtime.send_key_combination(runtime.engine.key_combos[i], runtime.userdata) {
			return false
		}
	}
	return true
}

steno_runtime_write_suggestion_line :: proc(runtime: ^Steno_Runtime, suggestion: ^Brevity_Suggestion) -> bool {
	if runtime.write_suggestion == nil {
		return true
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, "Suggestion: Use ")
	formatted_append_string(&buffer, suggestion.suggested_outline)
	formatted_append_string(&buffer, " for \"")
	formatted_append_string(&buffer, suggestion.text)
	formatted_append_string(&buffer, "\"\n")

	line, line_ok := clone_bytes_to_string(buffer[:])
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)
	return runtime.write_suggestion(line, runtime.userdata)
}

steno_runtime_write_suggestion_log_line :: proc(runtime: ^Steno_Runtime, suggestion: ^Brevity_Suggestion) -> bool {
	if runtime.write_suggestion_log == nil {
		return true
	}

	line, line_ok := brevity_suggestion_log_line(suggestion, time.time_to_unix(time.now()))
	if !line_ok {
		return false
	}
	defer owned_string_delete(line)

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, line)
	append(&buffer, '\n')
	log_line, log_line_ok := clone_bytes_to_string(buffer[:])
	if !log_line_ok {
		return false
	}
	defer owned_string_delete(log_line)
	return runtime.write_suggestion_log(log_line, runtime.userdata)
}

steno_runtime_maybe_emit_brevity_suggestion :: proc(runtime: ^Steno_Runtime) -> bool {
	if runtime.write_suggestion == nil && runtime.write_suggestion_log == nil {
		return true
	}

	suggestion, found := brevity_suggest(&runtime.engine)
	if !found {
		return true
	}
	defer brevity_suggestion_destroy(&suggestion)

	return steno_runtime_write_suggestion_log_line(runtime, &suggestion) &&
		steno_runtime_write_suggestion_line(runtime, &suggestion)
}

steno_runtime_handle_stroke :: proc(runtime: ^Steno_Runtime, stroke: Stroke_Input) -> bool {
	if runtime == nil || !runtime.session_active {
		return false
	}

	timing_started := steno_runtime_begin_translation_timing(runtime, stroke.received_ns)
	defer if timing_started {
		steno_runtime_cancel_translation_timing(runtime)
	}

	old_text, old_ok := simple_engine_render(&runtime.engine)
	if !old_ok {
		return false
	}
	defer owned_string_delete(old_text)
	first_combo := len(runtime.engine.key_combos)

	translated, maybe_suggest := steno_runtime_translate_stroke(runtime, stroke)
	if !translated {
		return false
	}

	new_text, new_ok := simple_engine_render(&runtime.engine)
	if !new_ok {
		return false
	}
	defer owned_string_delete(new_text)

	if !steno_runtime_replace_output_text(runtime, old_text, new_text) {
		return false
	}
	if !steno_runtime_send_key_combinations(runtime, first_combo) {
		return false
	}
	if maybe_suggest && !steno_runtime_maybe_emit_brevity_suggestion(runtime) {
		return false
	}

	steno_runtime_count_completed_stroke(runtime)
	return true
}

steno_runtime_handle_stroke_bits :: proc(runtime: ^Steno_Runtime, bits: u64) -> bool {
	return steno_runtime_handle_stroke(runtime, Stroke_Input{bits = bits})
}

steno_runtime_handle_active_stroke_bits :: proc(runtime: ^Steno_Runtime, bits: u64) -> bool {
	return steno_runtime_handle_stroke(runtime, Stroke_Input {
		bits = bits,
		phrase_namespace = steno_runtime_phrase_namespace_active(runtime),
		phrase_mode = steno_runtime_current_phrase_mode(runtime, true),
	})
}
