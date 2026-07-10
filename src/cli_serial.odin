package stoin

import "core:fmt"
import "core:os"
import "core:time"

serial_cli_resolve_serial_port :: proc(requested_port: string) -> (path: string, owned: bool, ok: bool) {
	if len(requested_port) > 0 {
		return requested_port, false, true
	}
	resolved_path, resolved := platform_serial_find_device()
	return resolved_path, true, resolved
}

tx_bolt_cli_handle_stroke :: proc(owner: ^Steno_Runtime_Owner, bits: u64, received_ns: u64 = 0) -> bool {
	if received_ns != 0 {
		return steno_runtime_owner_handle_active_stroke_bits_received(owner, bits, received_ns)
	}
	return steno_runtime_owner_handle_active_stroke_bits(owner, bits)
}

Multi_Tx_Bolt_Device :: struct {
	tx_bolt:      Tx_Bolt,
	serial:       Platform_Serial_Port,
	source_id:    int,
	last_byte_ms: u64,
}

multi_tx_bolt_path_connected :: proc(devices: []Multi_Tx_Bolt_Device, path: string) -> bool {
	for device in devices {
		if device.serial.port_path == path {
			return true
		}
	}
	return false
}

multi_tx_bolt_close_device :: proc(device: ^Multi_Tx_Bolt_Device) {
	if device == nil {
		return
	}
	platform_serial_close(&device.serial)
	device^ = {}
}

multi_tx_bolt_close_devices :: proc(devices: ^[dynamic]Multi_Tx_Bolt_Device) {
	if devices == nil {
		return
	}
	for i in 0..<len(devices^) {
		multi_tx_bolt_close_device(&devices^[i])
	}
	clear(devices)
}

multi_tx_bolt_connect_device :: proc(
	devices: ^[dynamic]Multi_Tx_Bolt_Device,
	path: string,
	baud_rate: int,
	next_source_id: ^int,
) -> bool {
	if devices == nil || next_source_id == nil || len(path) == 0 {
		return false
	}
	if len(devices^) >= TX_BOLT_MULTIPLE_MAX_DEVICES ||
	   multi_tx_bolt_path_connected(devices^[:], path) {
		return false
	}

	device: Multi_Tx_Bolt_Device
	if !platform_serial_open(&device.serial, path, baud_rate) {
		return false
	}
	device.source_id = next_source_id^
	next_source_id^ += 1
	append(devices, device)
	fmt.println("stoin: TX Bolt connected on", path)
	return true
}

multi_tx_bolt_scan_devices :: proc(
	config: ^Cli_Config,
	devices: ^[dynamic]Multi_Tx_Bolt_Device,
	baud_rate: int,
	next_source_id: ^int,
	announced_disconnected: ^bool,
) {
	if config == nil || devices == nil || next_source_id == nil {
		return
	}

	if len(config.serial_port_path) > 0 {
		if !multi_tx_bolt_path_connected(devices^[:], config.serial_port_path) {
			_ = multi_tx_bolt_connect_device(devices, config.serial_port_path, baud_rate, next_source_id)
		}
	} else {
		paths := platform_serial_find_devices()
		defer platform_serial_device_paths_destroy(&paths)
		for path in paths {
			if len(devices^) >= TX_BOLT_MULTIPLE_MAX_DEVICES {
				break
			}
			_ = multi_tx_bolt_connect_device(devices, path, baud_rate, next_source_id)
		}
	}

	if len(devices^) == 0 {
		if announced_disconnected != nil && !announced_disconnected^ {
			if len(config.serial_port_path) > 0 {
				fmt.eprintln("stoin: TX Bolt disconnected; waiting for", config.serial_port_path)
			} else {
				fmt.eprintln("stoin: TX Bolt disconnected; waiting for platform serial devices")
			}
			announced_disconnected^ = true
		}
	} else if announced_disconnected != nil {
		announced_disconnected^ = false
	}
}

tx_bolt_cli_process_merge_outputs :: proc(owner: ^Steno_Runtime_Owner, merge: ^Stroke_Merge) {
	if owner == nil || merge == nil {
		return
	}
	for {
		bits, ok := stroke_merge_next_output(merge)
		if !ok {
			return
		}
		_ = tx_bolt_cli_handle_stroke(owner, bits, cli_monotonic_ns())
	}
}

tx_bolt_cli_push_merged_stroke :: proc(
	owner: ^Steno_Runtime_Owner,
	merge: ^Stroke_Merge,
	source_id: int,
	bits: u64,
	window_ms: uint,
	now_ms: u64,
) {
	if owner == nil || merge == nil {
		return
	}
	stroke_merge_set_window_ms(merge, window_ms)
	_ = stroke_merge_push(merge, source_id, bits, now_ms)
	tx_bolt_cli_process_merge_outputs(owner, merge)
}

tx_bolt_cli_read_available :: proc(owner: ^Steno_Runtime_Owner, tx_bolt: ^Tx_Bolt, serial: ^Platform_Serial_Port, last_byte_ms: ^u64) -> (made_progress: bool) {
	for {
		value, read_result := platform_serial_read_byte(serial, 0)
		switch read_result {
		case .Byte:
			last_byte_ms^ = cli_monotonic_ms()
			made_progress = true
			if bits, decoded := tx_bolt_decode_byte(tx_bolt, value); decoded {
				_ = tx_bolt_cli_handle_stroke(owner, bits, cli_monotonic_ns())
			}
			continue
		case .None:
			return made_progress
		case .Error:
			return made_progress
		}
	}
}

tx_bolt_cli_flush_idle_stroke :: proc(owner: ^Steno_Runtime_Owner, tx_bolt: ^Tx_Bolt, last_byte_ms: ^u64) -> bool {
	if !tx_bolt_has_partial_stroke(tx_bolt) || last_byte_ms^ == 0 {
		return false
	}
	now_ms := cli_monotonic_ms()
	if now_ms - last_byte_ms^ < TX_BOLT_STROKE_IDLE_FLUSH_MS {
		return false
	}

	last_byte_ms^ = 0
	if bits, flushed := tx_bolt_flush_stroke(tx_bolt); flushed {
		_ = tx_bolt_cli_handle_stroke(owner, bits, cli_monotonic_ns())
		return true
	}
	return false
}

run_tx_bolt_multiple_cli :: proc(config: ^Cli_Config, owner: ^Steno_Runtime_Owner, baud_rate: int) -> bool {
	fmt.println("stoin: TX Bolt multiple-input serial capture starting at", baud_rate, "baud 8N1")
	fmt.println("stoin: multiple-input merge window is", config.multi_input_window_ms, "ms when 2+ devices are connected")
	fmt.println("stoin: loaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
	cli_print_phrase_toggle_status(config)
	cli_print_translation_timing_status(config)
	fmt.println("stoin: press Ctrl+C in this terminal to quit")
	if len(config.serial_port_path) > 0 {
		fmt.eprintln("stoin: --serial-port limits multiple-input mode to that single port")
	}

	devices := make([dynamic]Multi_Tx_Bolt_Device)
	defer {
		multi_tx_bolt_close_devices(&devices)
		delete(devices)
	}

	merge: Stroke_Merge
	stroke_merge_init(&merge, config.multi_input_window_ms)
	defer stroke_merge_destroy(&merge)

	next_source_id := 1
	announced_disconnected := false
	next_scan_ms: u64
	last_reload_poll_ms: u64

	for {
		cli_poll_runtime_reloads(owner, &last_reload_poll_ms)
		now_ms := cli_monotonic_ms()

		if now_ms >= next_scan_ms {
			multi_tx_bolt_scan_devices(
				config,
				&devices,
				baud_rate,
				&next_source_id,
				&announced_disconnected,
			)
			next_scan_ms = now_ms + TX_BOLT_MULTIPLE_SCAN_INTERVAL_MS
		}

		active_window_ms: uint
		if len(devices) > 1 {
			active_window_ms = config.multi_input_window_ms
		}
		stroke_merge_set_window_ms(&merge, active_window_ms)
		_ = stroke_merge_poll(&merge, now_ms)
		tx_bolt_cli_process_merge_outputs(owner, &merge)

		made_progress := false
		for i := 0; i < len(devices); {
			device := &devices[i]
			remove_device := false

			for {
				value, read_result := platform_serial_read_byte(&device.serial, 0)
				switch read_result {
				case .Byte:
					now_ms = cli_monotonic_ms()
					device.last_byte_ms = now_ms
					made_progress = true
					if bits, decoded := tx_bolt_decode_byte(&device.tx_bolt, value); decoded {
						tx_bolt_cli_push_merged_stroke(
							owner,
							&merge,
							device.source_id,
							bits,
							active_window_ms,
							now_ms,
						)
					}
					continue
				case .None:
				case .Error:
					remove_device = true
				}
				break
			}

			if !remove_device && !platform_serial_had_error(&device.serial) {
				now_ms = cli_monotonic_ms()
				if tx_bolt_has_partial_stroke(&device.tx_bolt) &&
				   device.last_byte_ms != 0 &&
				   now_ms - device.last_byte_ms >= TX_BOLT_STROKE_IDLE_FLUSH_MS {
					device.last_byte_ms = 0
					if bits, flushed := tx_bolt_flush_stroke(&device.tx_bolt); flushed {
						tx_bolt_cli_push_merged_stroke(
							owner,
							&merge,
							device.source_id,
							bits,
							active_window_ms,
							now_ms,
						)
						made_progress = true
					}
				}
			}

			if remove_device || platform_serial_had_error(&device.serial) || device.tx_bolt.had_error {
				fmt.println("stoin: TX Bolt disconnected from", device.serial.port_path, "; waiting for reconnect")
				multi_tx_bolt_close_device(device)
				ordered_remove(&devices, i)
				active_window_ms = 0
				if len(devices) > 1 {
					active_window_ms = config.multi_input_window_ms
				}
				stroke_merge_set_window_ms(&merge, active_window_ms)
				made_progress = true
				continue
			}

			i += 1
		}

		now_ms = cli_monotonic_ms()
		_ = stroke_merge_poll(&merge, now_ms)
		tx_bolt_cli_process_merge_outputs(owner, &merge)

		if !made_progress {
			cli_sleep_ms(TX_BOLT_MULTIPLE_LOOP_SLEEP_MS)
		}
	}
}

gemini_pr_cli_read_available :: proc(owner: ^Steno_Runtime_Owner, gemini: ^Gemini_Pr, serial: ^Platform_Serial_Port) -> (made_progress: bool) {
	for {
		value, read_result := platform_serial_read_byte(serial, 0)
		switch read_result {
		case .Byte:
			made_progress = true
			if bits, decoded := gemini_pr_decode_byte(gemini, value); decoded {
				_ = steno_runtime_owner_handle_active_stroke_bits_received(owner, bits, cli_monotonic_ns())
			}
			continue
		case .None:
			return made_progress
		case .Error:
			return made_progress
		}
	}
}

cli_monotonic_ms :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec / 1_000_000)
}

cli_monotonic_ns :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec)
}

cli_poll_runtime_reloads :: proc(owner: ^Steno_Runtime_Owner, last_poll_ms: ^u64) {
	if owner == nil || last_poll_ms == nil {
		return
	}
	now_ms := cli_monotonic_ms()
	if last_poll_ms^ != 0 && now_ms - last_poll_ms^ < RELOAD_POLL_INTERVAL_MS {
		return
	}
	last_poll_ms^ = now_ms
	_ = steno_runtime_owner_reload_files_if_changed(owner)
}

cli_sleep_ms :: proc(ms: int) {
	time.sleep(time.Duration(ms) * time.Millisecond)
}

run_tx_bolt_cli :: proc(config: ^Cli_Config) -> bool {
	when ODIN_OS != .Darwin && ODIN_OS != .Linux && ODIN_OS != .Windows {
		_ = config
		fmt.eprintln("stoin: TX Bolt input is currently implemented only on macOS, Linux, and Windows")
		return false
	} else {
		suggestion_log_file: ^os.File
		if len(config.suggestion_log_path) > 0 {
			open_file, open_err := os.open(
				config.suggestion_log_path,
				os.File_Flags{.Write, .Create, .Append},
				os.Permissions_Default_File,
			)
			if open_err != nil {
				fmt.eprintln("stoin: failed to open suggestion log")
				return false
			}
			suggestion_log_file = open_file
		}
		defer if suggestion_log_file != nil {
			os.close(suggestion_log_file)
		}

		output: Cli_Runtime_Output
		cli_runtime_output_init(&output)
		output.suggestion_log_file = suggestion_log_file
		defer cli_runtime_output_destroy(&output)

		runtime_config := Steno_Runtime_Load_Config {
			dictionary_paths = config.dict_paths[:],
			dictionary_enabled = config.dict_enabled[:],
			orthography_path = config.orthography_path,
			userdata = rawptr(&output),
		}
		if config.trace_strokes {
			runtime_config.write_trace = cli_runtime_write_line
		}
		_ = cli_platform_configure_runtime_output(&runtime_config, config)
		if len(config.phrasing_path) > 0 {
			runtime_config.phrasing_path = config.phrasing_path
		}
		if config.print_suggestions {
			runtime_config.write_suggestion = cli_runtime_write_suggestion
		}
		if suggestion_log_file != nil {
			runtime_config.write_suggestion_log = cli_runtime_write_suggestion_log
		}

		owner: Steno_Runtime_Owner
		if !cli_runtime_owner_init(&owner, &runtime_config) {
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_runtime_input_options(config, &owner.runtime)

		if !cli_platform_output_init() {
			fmt.eprintln("stoin: failed to initialize", cli_platform_live_output_name())
			return false
		}
		cli_platform_translation_timing_set_enabled(config.time_translations)
		defer cli_platform_output_shutdown()
		defer cli_platform_translation_timing_set_enabled(false)
		if cli_keyboard_listener_enabled(config) {
			if !cli_platform_keyboard_listener_supported() {
				fmt.eprintln("stoin: phrase-toggle and trace-key-events keyboard listening are not implemented on this platform yet")
				return false
			}
			if !cli_platform_keyboard_listener_start(&owner) {
				fmt.eprintln("stoin: failed to start platform keyboard listener")
				return false
			}
			defer cli_platform_keyboard_listener_stop()
		}

		baud_rate := config.serial_baud_rate
		if baud_rate == 0 {
			baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
		}
		if config.multiple_inputs {
			return run_tx_bolt_multiple_cli(config, &owner, baud_rate)
		}

		fmt.println("stoin: TX Bolt serial capture starting at", baud_rate, "baud 8N1")
		fmt.println("stoin: loaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
		cli_print_phrase_toggle_status(config)
		cli_print_translation_timing_status(config)
		fmt.println("stoin: press Ctrl+C in this terminal to quit")

		tx_bolt: Tx_Bolt
		serial: Platform_Serial_Port
		connected := false
		announced_disconnected := false
		last_byte_ms: u64
		last_reload_poll_ms: u64

		for {
			cli_poll_runtime_reloads(&owner, &last_reload_poll_ms)

			if !connected {
				resolved_port_path, resolved_path_owned, resolved := serial_cli_resolve_serial_port(config.serial_port_path)
				if !resolved {
					if !announced_disconnected {
						if len(config.serial_port_path) > 0 {
							fmt.eprintln("stoin: TX Bolt disconnected; waiting for", config.serial_port_path)
						} else {
							fmt.eprintln("stoin: TX Bolt disconnected; waiting for a platform default serial device")
						}
						announced_disconnected = true
					}
					cli_sleep_ms(1000)
					continue
				}

				if !platform_serial_open(&serial, resolved_port_path, baud_rate) {
					if !announced_disconnected {
						fmt.eprintln("stoin: TX Bolt disconnected; waiting for", resolved_port_path)
						announced_disconnected = true
					}
					if resolved_path_owned {
						owned_string_delete(resolved_port_path)
					}
					cli_sleep_ms(1000)
					continue
				}
				if resolved_path_owned {
					owned_string_delete(resolved_port_path)
				}

				tx_bolt = {}
				connected = true
				announced_disconnected = false
				last_byte_ms = 0
				fmt.println("stoin: TX Bolt connected on", serial.port_path)
			}

			made_progress := tx_bolt_cli_read_available(&owner, &tx_bolt, &serial, &last_byte_ms)
			if !platform_serial_had_error(&serial) && tx_bolt_cli_flush_idle_stroke(&owner, &tx_bolt, &last_byte_ms) {
				made_progress = true
			}

			if platform_serial_had_error(&serial) {
				fmt.println("stoin: TX Bolt disconnected from", serial.port_path, "; waiting for reconnect")
				platform_serial_close(&serial)
				tx_bolt = {}
				connected = false
				last_byte_ms = 0
				cli_sleep_ms(1000)
			} else if !made_progress {
				cli_sleep_ms(INPUT_EVENT_POLL_SLEEP_MS)
			}
		}
	}
}

run_gemini_pr_cli :: proc(config: ^Cli_Config) -> bool {
	when ODIN_OS != .Darwin && ODIN_OS != .Linux && ODIN_OS != .Windows {
		_ = config
		fmt.eprintln("stoin: Gemini PR input is currently implemented only on macOS, Linux, and Windows")
		return false
	} else {
		suggestion_log_file: ^os.File
		if len(config.suggestion_log_path) > 0 {
			open_file, open_err := os.open(
				config.suggestion_log_path,
				os.File_Flags{.Write, .Create, .Append},
				os.Permissions_Default_File,
			)
			if open_err != nil {
				fmt.eprintln("stoin: failed to open suggestion log")
				return false
			}
			suggestion_log_file = open_file
		}
		defer if suggestion_log_file != nil {
			os.close(suggestion_log_file)
		}

		output: Cli_Runtime_Output
		cli_runtime_output_init(&output)
		output.suggestion_log_file = suggestion_log_file
		defer cli_runtime_output_destroy(&output)

		runtime_config := Steno_Runtime_Load_Config {
			dictionary_paths = config.dict_paths[:],
			dictionary_enabled = config.dict_enabled[:],
			orthography_path = config.orthography_path,
			userdata = rawptr(&output),
		}
		if config.trace_strokes {
			runtime_config.write_trace = cli_runtime_write_line
		}
		_ = cli_platform_configure_runtime_output(&runtime_config, config)
		if len(config.phrasing_path) > 0 {
			runtime_config.phrasing_path = config.phrasing_path
		}
		if config.print_suggestions {
			runtime_config.write_suggestion = cli_runtime_write_suggestion
		}
		if suggestion_log_file != nil {
			runtime_config.write_suggestion_log = cli_runtime_write_suggestion_log
		}

		owner: Steno_Runtime_Owner
		if !cli_runtime_owner_init(&owner, &runtime_config) {
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_runtime_input_options(config, &owner.runtime)

		if !cli_platform_output_init() {
			fmt.eprintln("stoin: failed to initialize", cli_platform_live_output_name())
			return false
		}
		cli_platform_translation_timing_set_enabled(config.time_translations)
		defer cli_platform_output_shutdown()
		defer cli_platform_translation_timing_set_enabled(false)
		if cli_keyboard_listener_enabled(config) {
			if !cli_platform_keyboard_listener_supported() {
				fmt.eprintln("stoin: phrase-toggle and trace-key-events keyboard listening are not implemented on this platform yet")
				return false
			}
			if !cli_platform_keyboard_listener_start(&owner) {
				fmt.eprintln("stoin: failed to start platform keyboard listener")
				return false
			}
			defer cli_platform_keyboard_listener_stop()
		}

		baud_rate := config.serial_baud_rate
		if baud_rate == 0 {
			baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
		}
		fmt.println("stoin: Gemini PR serial capture starting at", baud_rate, "baud 8N1")
		fmt.println("stoin: loaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
		cli_print_phrase_toggle_status(config)
		cli_print_translation_timing_status(config)
		fmt.println("stoin: press Ctrl+C in this terminal to quit")

		gemini: Gemini_Pr
		serial: Platform_Serial_Port
		connected := false
		announced_disconnected := false
		last_reload_poll_ms: u64

		for {
			cli_poll_runtime_reloads(&owner, &last_reload_poll_ms)

			if !connected {
				resolved_port_path, resolved_path_owned, resolved := serial_cli_resolve_serial_port(config.serial_port_path)
				if !resolved {
					if !announced_disconnected {
						if len(config.serial_port_path) > 0 {
							fmt.eprintln("stoin: Gemini PR disconnected; waiting for", config.serial_port_path)
						} else {
							fmt.eprintln("stoin: Gemini PR disconnected; waiting for a platform default serial device")
						}
						announced_disconnected = true
					}
					cli_sleep_ms(1000)
					continue
				}

				if !platform_serial_open(&serial, resolved_port_path, baud_rate) {
					if !announced_disconnected {
						fmt.eprintln("stoin: Gemini PR disconnected; waiting for", resolved_port_path)
						announced_disconnected = true
					}
					if resolved_path_owned {
						owned_string_delete(resolved_port_path)
					}
					cli_sleep_ms(1000)
					continue
				}
				if resolved_path_owned {
					owned_string_delete(resolved_port_path)
				}

				gemini = {}
				connected = true
				announced_disconnected = false
				fmt.println("stoin: Gemini PR connected on", serial.port_path)
			}

			made_progress := gemini_pr_cli_read_available(&owner, &gemini, &serial)
			if platform_serial_had_error(&serial) {
				fmt.println("stoin: Gemini PR disconnected from", serial.port_path, "; waiting for reconnect")
				platform_serial_close(&serial)
				gemini = {}
				connected = false
				cli_sleep_ms(1000)
			} else if !made_progress {
				cli_sleep_ms(INPUT_EVENT_POLL_SLEEP_MS)
			}
		}
	}
}

stentura_cli_open :: proc(config: ^Cli_Config, stentura: ^Stentura, baud_rate: int) -> bool {
	if len(config.serial_port_path) > 0 {
		return stentura_open(stentura, config.serial_port_path, baud_rate)
	}

	paths := platform_serial_find_devices()
	defer platform_serial_device_paths_destroy(&paths)
	for path in paths {
		if stentura_open(stentura, path, baud_rate) {
			return true
		}
	}
	return false
}

run_stentura_cli :: proc(config: ^Cli_Config) -> bool {
	when ODIN_OS != .Darwin && ODIN_OS != .Linux && ODIN_OS != .Windows {
		_ = config
		fmt.eprintln("stoin: Stentura input is currently implemented only on macOS, Linux, and Windows")
		return false
	} else {
		suggestion_log_file: ^os.File
		if len(config.suggestion_log_path) > 0 {
			open_file, open_err := os.open(
				config.suggestion_log_path,
				os.File_Flags{.Write, .Create, .Append},
				os.Permissions_Default_File,
			)
			if open_err != nil {
				fmt.eprintln("stoin: failed to open suggestion log")
				return false
			}
			suggestion_log_file = open_file
		}
		defer if suggestion_log_file != nil {
			os.close(suggestion_log_file)
		}

		output: Cli_Runtime_Output
		cli_runtime_output_init(&output)
		output.suggestion_log_file = suggestion_log_file
		defer cli_runtime_output_destroy(&output)

		runtime_config := Steno_Runtime_Load_Config {
			dictionary_paths = config.dict_paths[:],
			dictionary_enabled = config.dict_enabled[:],
			orthography_path = config.orthography_path,
			userdata = rawptr(&output),
		}
		if config.trace_strokes {
			runtime_config.write_trace = cli_runtime_write_line
		}
		_ = cli_platform_configure_runtime_output(&runtime_config, config)
		if len(config.phrasing_path) > 0 {
			runtime_config.phrasing_path = config.phrasing_path
		}
		if config.print_suggestions {
			runtime_config.write_suggestion = cli_runtime_write_suggestion
		}
		if suggestion_log_file != nil {
			runtime_config.write_suggestion_log = cli_runtime_write_suggestion_log
		}

		owner: Steno_Runtime_Owner
		if !cli_runtime_owner_init(&owner, &runtime_config) {
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_runtime_input_options(config, &owner.runtime)

		if !cli_platform_output_init() {
			fmt.eprintln("stoin: failed to initialize", cli_platform_live_output_name())
			return false
		}
		cli_platform_translation_timing_set_enabled(config.time_translations)
		defer cli_platform_output_shutdown()
		defer cli_platform_translation_timing_set_enabled(false)
		if cli_keyboard_listener_enabled(config) {
			if !cli_platform_keyboard_listener_supported() {
				fmt.eprintln("stoin: phrase-toggle and trace-key-events keyboard listening are not implemented on this platform yet")
				return false
			}
			if !cli_platform_keyboard_listener_start(&owner) {
				fmt.eprintln("stoin: failed to start platform keyboard listener")
				return false
			}
			defer cli_platform_keyboard_listener_stop()
		}

		baud_rate := config.serial_baud_rate
		if baud_rate == 0 {
			baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
		}
		fmt.println("stoin: Stentura serial capture starting at", baud_rate, "baud 8N1")
		fmt.println("stoin: loaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
		cli_print_phrase_toggle_status(config)
		cli_print_translation_timing_status(config)
		fmt.println("stoin: press Ctrl+C in this terminal to quit")

		stentura: Stentura
		connected := false
		announced_disconnected := false
		last_reload_poll_ms: u64

		for {
			cli_poll_runtime_reloads(&owner, &last_reload_poll_ms)

			if !connected {
				if !stentura_cli_open(config, &stentura, baud_rate) {
					if !announced_disconnected {
						reason := stentura_status_message(&stentura)
						if len(reason) > 0 {
							fmt.eprintln("stoin: Stentura connection failed:", reason)
						}
						if len(config.serial_port_path) > 0 {
							fmt.eprintln("stoin: Stentura disconnected; waiting for", config.serial_port_path)
						} else {
							fmt.eprintln("stoin: Stentura disconnected; waiting for a Stentura-compatible serial device")
						}
						announced_disconnected = true
					}
					cli_sleep_ms(1000)
					continue
				}

				connected = true
				announced_disconnected = false
				fmt.println("stoin: Stentura connected on", stentura_port_path(&stentura))
			}

			if bits, read := stentura_read_stroke(&stentura); read {
				_ = steno_runtime_owner_handle_active_stroke_bits_received(&owner, bits, cli_monotonic_ns())
			} else if stentura_had_error(&stentura) {
				fmt.println("stoin: Stentura disconnected from", stentura_port_path(&stentura), "; waiting for reconnect")
				stentura_close(&stentura)
				connected = false
				cli_sleep_ms(1000)
			} else {
				cli_sleep_ms(INPUT_EVENT_POLL_SLEEP_MS)
			}
		}
	}
}

raw_serial_byte_is_printable :: proc(value: byte) -> bool {
	return value >= 32 && value <= 126
}

raw_serial_print_burst :: proc(bytes: []byte) {
	if len(bytes) == 0 {
		return
	}

	suffix := "s"
	if len(bytes) == 1 {
		suffix = ""
	}
	fmt.printf("stoin: raw %d byte%s:", len(bytes), suffix)
	for value in bytes {
		fmt.printf(" %02X", value)
	}
	fmt.print(" | ")
	for value in bytes {
		if raw_serial_byte_is_printable(value) {
			fmt.printf("%c", value)
		} else {
			fmt.print(".")
		}
	}
	fmt.println()
}

raw_serial_flush_burst :: proc(burst: ^[RAW_SERIAL_BURST_CAPACITY]byte, burst_count: ^int) {
	if burst_count^ == 0 {
		return
	}
	raw_serial_print_burst(burst[:burst_count^])
	burst_count^ = 0
}

run_raw_serial_cli :: proc(config: ^Cli_Config) -> bool {
	baud_rate := config.serial_baud_rate
	if baud_rate == 0 {
		baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
	}
	fmt.println("stoin: raw serial dump starting at", baud_rate, "baud 8N1")
	fmt.println("stoin: dictionary, text output, and keyboard capture are disabled in this mode")
	fmt.println("stoin: press Ctrl+C in this terminal to quit")

	serial: Platform_Serial_Port
	connected := false
	announced_disconnected := false
	burst: [RAW_SERIAL_BURST_CAPACITY]byte
	burst_count := 0

	for {
		if !connected {
			resolved_port_path, resolved_path_owned, resolved := serial_cli_resolve_serial_port(config.serial_port_path)
			if !resolved {
				if !announced_disconnected {
					if len(config.serial_port_path) > 0 {
						fmt.eprintln("stoin: raw serial disconnected; waiting for", config.serial_port_path)
					} else {
						fmt.eprintln("stoin: raw serial disconnected; waiting for a platform default serial device")
					}
					announced_disconnected = true
				}
				cli_sleep_ms(1000)
				continue
			}

			if !platform_serial_open(&serial, resolved_port_path, baud_rate) {
				if !announced_disconnected {
					fmt.eprintln("stoin: raw serial disconnected; waiting for", resolved_port_path)
					announced_disconnected = true
				}
				if resolved_path_owned {
					owned_string_delete(resolved_port_path)
				}
				cli_sleep_ms(1000)
				continue
			}
			if resolved_path_owned {
				owned_string_delete(resolved_port_path)
			}

			connected = true
			announced_disconnected = false
			burst_count = 0
			fmt.println("stoin: raw serial connected on", serial.port_path)
		}

		value, read_result := platform_serial_read_byte(&serial, 100)
		switch read_result {
		case .Byte:
			if burst_count == len(burst) {
				raw_serial_flush_burst(&burst, &burst_count)
			}
			burst[burst_count] = value
			burst_count += 1
		case .None:
			raw_serial_flush_burst(&burst, &burst_count)
		case .Error:
			raw_serial_flush_burst(&burst, &burst_count)
			if platform_serial_had_error(&serial) {
				fmt.println("stoin: raw serial disconnected from", serial.port_path, "; waiting for reconnect")
			}
			platform_serial_close(&serial)
			connected = false
			cli_sleep_ms(1000)
		}
	}
}
