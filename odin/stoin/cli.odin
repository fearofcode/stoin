package stoin

import "core:fmt"
import "core:os"
import "core:time"

APP_NAME :: "stoin"
INPUT_EVENT_POLL_SLEEP_MS :: 10
TX_BOLT_STROKE_IDLE_FLUSH_MS :: 100
RAW_SERIAL_BURST_CAPACITY :: 256

Cli_Mode :: enum {
	Scaffold,
	Help,
	Lookup,
	Translate,
	Qwerty,
	Tx_Bolt,
	Raw_Serial,
}

Cli_Config :: struct {
	mode:          Cli_Mode,
	dict_paths:    [dynamic]string,
	lookups:       [dynamic]string,
	translates:     [dynamic]string,
	input_qwerty:   bool,
	input_tx_bolt:  bool,
	raw_serial:     bool,
	keymap_path:    string,
	serial_port_path: string,
	serial_baud_rate: int,
	phrase_toggle_enabled: bool,
	phrase_toggle_keycode: u16,
	phrase_toggle_name: string,
	nonverb_phrase_toggle_enabled: bool,
	nonverb_phrase_toggle_keycode: u16,
	nonverb_phrase_toggle_name: string,
	print_suggestions: bool,
	suggestion_log_path: string,
	orthography_path: string,
	phrasing_path: string,
	phrase_mode: Phrase_Lookup_Mode,
	phrase_mode_enabled: bool,
	error_message: string,
}

parse_cli_phrase_mode :: proc(text: string) -> (Phrase_Lookup_Mode, bool) {
	switch text {
	case "all":
		return .All, true
	case "verbs", "verb":
		return .Verbs, true
	case "nonverbs", "nonverb":
		return .Nonverbs, true
	}
	return .All, false
}

parse_cli_args :: proc(args: []string) -> (config: Cli_Config, ok: bool) {
	config.mode = .Scaffold
	config.dict_paths = make([dynamic]string)
	config.lookups = make([dynamic]string)
	config.translates = make([dynamic]string)
	config.phrase_mode = .All
	config.serial_baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD

	for i := 1; i < len(args); i += 1 {
		arg := args[i]
		switch arg {
		case "--help", "-h":
			config.mode = .Help
		case "--dict":
			if i + 1 >= len(args) {
				config.error_message = "--dict requires a path"
				return config, false
			}
			append(&config.dict_paths, args[i + 1])
			i += 1
		case "--lookup":
			if i + 1 >= len(args) {
				config.error_message = "--lookup requires an outline"
				return config, false
			}
			append(&config.lookups, args[i + 1])
			i += 1
		case "--raw-serial", "--dump-serial":
			config.raw_serial = true
		case "--input":
			if i + 1 >= len(args) {
				config.error_message = "--input requires qwerty"
				return config, false
			}
			input_mode := args[i + 1]
			if input_mode == "qwerty" {
				config.input_qwerty = true
			} else if input_mode == "tx-bolt" || input_mode == "txbolt" || input_mode == "bolt" {
				config.input_tx_bolt = true
			} else {
				config.error_message = "--input currently supports qwerty or tx-bolt"
				return config, false
			}
			i += 1
		case "--keymap":
			if i + 1 >= len(args) {
				config.error_message = "--keymap requires a path"
				return config, false
			}
			config.keymap_path = args[i + 1]
			i += 1
		case "--serial-port", "--port", "--tx-bolt-port":
			if i + 1 >= len(args) {
				config.error_message = "--serial-port requires a path"
				return config, false
			}
			config.serial_port_path = args[i + 1]
			i += 1
		case "--serial-baud", "--baud", "--tx-bolt-baud":
			if i + 1 >= len(args) {
				config.error_message = "--serial-baud requires a baud rate"
				return config, false
			}
			baud_rate, baud_ok := parse_positive_int(args[i + 1])
			if !baud_ok {
				config.error_message = "--serial-baud requires a positive baud rate"
				return config, false
			}
			config.serial_baud_rate = baud_rate
			i += 1
		case "--phrase-toggle", "--phase-toggle":
			if i + 1 >= len(args) {
				config.error_message = "--phrase-toggle requires a key"
				return config, false
			}
			key_name := args[i + 1]
			keycode, keycode_ok := keycode_from_name(key_name)
			if !keycode_ok {
				config.error_message = "unknown phrase toggle key"
				return config, false
			}
			config.phrase_toggle_enabled = true
			config.phrase_toggle_keycode = keycode
			config.phrase_toggle_name = key_name
			i += 1
		case "--nonverb-phrase-toggle", "--nonverb-phase-toggle", "--nonverb-toggle":
			if i + 1 >= len(args) {
				config.error_message = "--nonverb-phrase-toggle requires a key"
				return config, false
			}
			key_name := args[i + 1]
			keycode, keycode_ok := keycode_from_name(key_name)
			if !keycode_ok {
				config.error_message = "unknown nonverb phrase toggle key"
				return config, false
			}
			config.nonverb_phrase_toggle_enabled = true
			config.nonverb_phrase_toggle_keycode = keycode
			config.nonverb_phrase_toggle_name = key_name
			i += 1
		case "--print-suggestions":
			config.print_suggestions = true
		case "--suggestion-log":
			if i + 1 >= len(args) {
				config.error_message = "--suggestion-log requires a path"
				return config, false
			}
			config.suggestion_log_path = args[i + 1]
			i += 1
		case "--orthography":
			if i + 1 >= len(args) {
				config.error_message = "--orthography requires a path"
				return config, false
			}
			config.orthography_path = args[i + 1]
			i += 1
		case "--phrasing":
			if i + 1 >= len(args) {
				config.error_message = "--phrasing requires a path"
				return config, false
			}
			config.phrasing_path = args[i + 1]
			i += 1
		case "--phrase-mode":
			if i + 1 >= len(args) {
				config.error_message = "--phrase-mode requires all, verbs, or nonverbs"
				return config, false
			}
			phrase_mode, phrase_mode_ok := parse_cli_phrase_mode(args[i + 1])
			if !phrase_mode_ok {
				config.error_message = "--phrase-mode requires all, verbs, or nonverbs"
				return config, false
			}
			config.phrase_mode = phrase_mode
			config.phrase_mode_enabled = true
			i += 1
		case "--translate":
			if i + 1 >= len(args) {
				config.error_message = "--translate requires at least one outline"
				return config, false
			}
			for j := i + 1; j < len(args); j += 1 {
				append(&config.translates, args[j])
			}
			i = len(args)
		case:
			config.error_message = "unknown argument"
			return config, false
		}
	}

	selected_modes := 0
	if len(config.lookups) > 0 {
		selected_modes += 1
	}
	if len(config.translates) > 0 {
		selected_modes += 1
	}
	if config.input_qwerty {
		selected_modes += 1
	}
	if config.input_tx_bolt {
		selected_modes += 1
	}
	if config.raw_serial {
		selected_modes += 1
	}
	if selected_modes > 1 {
		config.error_message = "--lookup, --translate, --input, and --raw-serial cannot be combined"
		return config, false
	}

	if config.phrase_toggle_enabled &&
	   config.nonverb_phrase_toggle_enabled &&
	   config.phrase_toggle_keycode == config.nonverb_phrase_toggle_keycode {
		config.error_message = "--phrase-toggle and --nonverb-phrase-toggle must use distinct keys"
		return config, false
	}

	if config.raw_serial {
		config.mode = .Raw_Serial
	} else if config.input_qwerty {
		config.mode = .Qwerty
		if len(config.dict_paths) == 0 {
			config.error_message = "--input qwerty requires at least one --dict"
			return config, false
		}
		if len(config.keymap_path) == 0 {
			config.keymap_path = "stoin.keymap"
		}
		if config.phrase_mode_enabled && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-mode requires --phrasing"
			return config, false
		}
		if cli_phrase_toggles_enabled(&config) && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-toggle requires --phrasing"
			return config, false
		}
	} else if config.input_tx_bolt {
		config.mode = .Tx_Bolt
		if len(config.dict_paths) == 0 {
			config.error_message = "--input tx-bolt requires at least one --dict"
			return config, false
		}
		if config.phrase_mode_enabled && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-mode requires --phrasing"
			return config, false
		}
		if cli_phrase_toggles_enabled(&config) && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-toggle requires --phrasing"
			return config, false
		}
	} else if len(config.lookups) > 0 {
		config.mode = .Lookup
		if len(config.dict_paths) == 0 {
			config.error_message = "--lookup requires at least one --dict"
			return config, false
		}
	} else if len(config.translates) > 0 {
		config.mode = .Translate
		if len(config.dict_paths) == 0 {
			config.error_message = "--translate requires at least one --dict"
			return config, false
		}
		if config.phrase_mode_enabled && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-mode requires --phrasing"
			return config, false
		}
	} else if len(config.dict_paths) > 0 {
		config.error_message = "--dict requires --lookup or --translate"
		return config, false
	}

	return config, true
}

cli_phrase_toggles_enabled :: proc(config: ^Cli_Config) -> bool {
	return config != nil && (config.phrase_toggle_enabled || config.nonverb_phrase_toggle_enabled)
}

cli_configure_phrase_toggles :: proc(config: ^Cli_Config, runtime: ^Steno_Runtime) {
	if config == nil || runtime == nil {
		return
	}
	steno_runtime_configure_phrase_toggles(
		runtime,
		config.phrase_toggle_enabled,
		config.phrase_toggle_keycode,
		config.nonverb_phrase_toggle_enabled,
		config.nonverb_phrase_toggle_keycode,
	)
}

cli_print_phrase_toggle_status :: proc(config: ^Cli_Config) {
	if config == nil {
		return
	}
	if config.phrase_toggle_enabled {
		prefix := ""
		if config.nonverb_phrase_toggle_enabled {
			prefix = "verb "
		}
		fmt.printf(
			"stoin: %sphrase toggle %s enabled (keycode %d)\n",
			prefix,
			config.phrase_toggle_name,
			config.phrase_toggle_keycode,
		)
	}
	if config.nonverb_phrase_toggle_enabled {
		fmt.printf(
			"stoin: nonverb phrase toggle %s enabled (keycode %d)\n",
			config.nonverb_phrase_toggle_name,
			config.nonverb_phrase_toggle_keycode,
		)
	}
}

cli_config_destroy :: proc(config: ^Cli_Config) {
	delete(config.dict_paths)
	delete(config.lookups)
	delete(config.translates)
	config^ = {}
}

run_lookup_cli :: proc(config: ^Cli_Config) -> bool {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)

	if !dictionary_load_many(&dictionary, config.dict_paths[:]) {
		fmt.eprintln("stoin: failed to load dictionary")
		return false
	}

	all_ok := true
	for outline in config.lookups {
		if translation, found := dictionary_lookup_stroke(&dictionary, outline); found {
			cli_write(outline)
			cli_write(" -> ")
			cli_write_line(translation)
		} else {
			fmt.eprintln("stoin: no exact dictionary entry for", outline)
			all_ok = false
		}
	}
	return all_ok
}

Cli_Runtime_Output :: struct {
	text:                [dynamic]byte,
	suggestion_log_file: ^os.File,
}

cli_runtime_output_init :: proc(output: ^Cli_Runtime_Output) {
	output^ = {}
	output.text = make([dynamic]byte)
}

cli_runtime_output_destroy :: proc(output: ^Cli_Runtime_Output) {
	delete(output.text)
	output^ = {}
}

cli_runtime_send_text :: proc(text: string, userdata: rawptr) -> bool {
	output := (^Cli_Runtime_Output)(userdata)
	if output == nil {
		return false
	}
	for b in transmute([]byte)text {
		append(&output.text, b)
	}
	return true
}

cli_runtime_delete_text :: proc(text: string, userdata: rawptr) -> bool {
	output := (^Cli_Runtime_Output)(userdata)
	if output == nil || len(text) > len(output.text) {
		return false
	}
	start := len(output.text) - len(text)
	if string(output.text[start:]) != text {
		return false
	}
	resize(&output.text, start)
	return true
}

cli_runtime_send_key_combination :: proc(combo: string, userdata: rawptr) -> bool {
	_ = combo
	_ = userdata
	return true
}

cli_runtime_write_line :: proc(line: string, userdata: rawptr) -> bool {
	_ = userdata
	cli_write(line)
	return true
}

cli_runtime_write_suggestion :: proc(line: string, userdata: rawptr) -> bool {
	return cli_runtime_write_line(line, userdata)
}

cli_runtime_write_suggestion_log :: proc(line: string, userdata: rawptr) -> bool {
	output := (^Cli_Runtime_Output)(userdata)
	if output == nil || output.suggestion_log_file == nil {
		return false
	}
	_, write_err := os.write_string(output.suggestion_log_file, line)
	return write_err == nil
}

run_translate_cli :: proc(config: ^Cli_Config) -> bool {
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
		orthography_path = config.orthography_path,
		send_text = cli_runtime_send_text,
		delete_text = cli_runtime_delete_text,
		send_key_combination = cli_runtime_send_key_combination,
		userdata = rawptr(&output),
	}
	if config.phrase_mode_enabled {
		runtime_config.phrasing_path = config.phrasing_path
	}
	if config.print_suggestions {
		runtime_config.write_suggestion = cli_runtime_write_suggestion
	}
	if suggestion_log_file != nil {
		runtime_config.write_suggestion_log = cli_runtime_write_suggestion_log
	}

	owner: Steno_Runtime_Owner
	if !steno_runtime_owner_init(&owner, &runtime_config) {
		fmt.eprintln("stoin: failed to initialize runtime")
		return false
	}
	defer steno_runtime_owner_destroy(&owner)

	for outline in config.translates {
		bits, parsed := stroke_string_to_bits(outline)
		translated := false
		if parsed {
			if config.phrase_mode_enabled {
				translated = steno_runtime_owner_handle_stroke(&owner, Stroke_Input {
					bits = bits,
					phrase_namespace = true,
					phrase_mode = steno_phrase_mode_from_lookup_mode(config.phrase_mode),
				})
			} else {
				translated = steno_runtime_owner_handle_stroke_bits(&owner, bits)
			}
		}
		if !parsed || !translated {
			fmt.eprintln("stoin: failed to translate outline sequence")
			return false
		}
	}

	for outline, i in config.translates {
		if i != 0 {
			cli_write("/")
		}
		cli_write(outline)
	}
	cli_write(" -> ")
	cli_write_line(string(output.text[:]))
	return true
}

run_qwerty_cli :: proc(config: ^Cli_Config) -> bool {
	when ODIN_OS != .Darwin {
		_ = config
		fmt.eprintln("stoin: qwerty input is currently implemented only on macOS in the Odin port")
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
			keymap_path = config.keymap_path,
			orthography_path = config.orthography_path,
			send_text = macos_runtime_send_text,
			delete_text = macos_runtime_delete_text,
			send_key_combination = macos_runtime_send_key_combination,
			write_trace = cli_runtime_write_line,
			userdata = rawptr(&output),
		}
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
		if !steno_runtime_owner_init(&owner, &runtime_config) {
			fmt.eprintln("stoin: failed to initialize runtime")
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_phrase_toggles(config, &owner.runtime)

		if !macos_qwerty_start(&owner) {
			fmt.eprintln("stoin: failed to start macOS qwerty event tap; confirm Accessibility permission")
			return false
		}
		defer macos_output_shutdown()
		defer macos_qwerty_stop()

		fmt.eprintln("stoin: macOS Odin qwerty event tap running")
		fmt.eprintln(
			"stoin: loaded",
			keymap_binding_count(&owner.keymap),
			"key bindings and",
			dictionary_count(&owner.dictionary_stack.dictionary),
			"dictionary entries",
		)
		cli_print_phrase_toggle_status(config)
		fmt.eprintln("stoin: press Ctrl+Esc to toggle capture; press Ctrl+C in this terminal to quit")
		macos_qwerty_run()
		return true
	}
}

serial_cli_resolve_serial_port :: proc(requested_port: string) -> (path: string, owned: bool, ok: bool) {
	if len(requested_port) > 0 {
		return requested_port, false, true
	}
	resolved_path, resolved := platform_serial_find_device()
	return resolved_path, true, resolved
}

tx_bolt_cli_handle_stroke :: proc(owner: ^Steno_Runtime_Owner, bits: u64) -> bool {
	return steno_runtime_owner_handle_active_stroke_bits(owner, bits)
}

tx_bolt_cli_read_available :: proc(owner: ^Steno_Runtime_Owner, tx_bolt: ^Tx_Bolt, serial: ^Platform_Serial_Port, last_byte_ms: ^u64) -> (made_progress: bool) {
	for {
		value, read_result := platform_serial_read_byte(serial, 0)
		switch read_result {
		case .Byte:
			last_byte_ms^ = cli_monotonic_ms()
			made_progress = true
			if bits, decoded := tx_bolt_decode_byte(tx_bolt, value); decoded {
				_ = tx_bolt_cli_handle_stroke(owner, bits)
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
		_ = tx_bolt_cli_handle_stroke(owner, bits)
		return true
	}
	return false
}

cli_monotonic_ms :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec / 1_000_000)
}

cli_sleep_ms :: proc(ms: int) {
	time.sleep(time.Duration(ms) * time.Millisecond)
}

run_tx_bolt_cli :: proc(config: ^Cli_Config) -> bool {
	when ODIN_OS != .Darwin {
		_ = config
		fmt.eprintln("stoin: TX Bolt input is currently implemented only on macOS in the Odin port")
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
			orthography_path = config.orthography_path,
			send_text = macos_runtime_send_text,
			delete_text = macos_runtime_delete_text,
			send_key_combination = macos_runtime_send_key_combination,
			write_trace = cli_runtime_write_line,
			userdata = rawptr(&output),
		}
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
		if !steno_runtime_owner_init(&owner, &runtime_config) {
			fmt.eprintln("stoin: failed to initialize runtime")
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_phrase_toggles(config, &owner.runtime)

		if !macos_output_init() {
			fmt.eprintln("stoin: failed to initialize macOS text output")
			return false
		}
		defer macos_output_shutdown()
		if cli_phrase_toggles_enabled(config) {
			if !macos_keyboard_listen_start(&owner) {
				fmt.eprintln("stoin: failed to start macOS phrase toggle listener; confirm Accessibility permission")
				return false
			}
			defer macos_keyboard_listen_stop()
		}

		baud_rate := config.serial_baud_rate
		if baud_rate == 0 {
			baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
		}
		fmt.println("stoin: TX Bolt serial capture starting at", baud_rate, "baud 8N1")
		fmt.println("stoin: loaded", dictionary_count(&owner.dictionary_stack.dictionary), "dictionary entries")
		cli_print_phrase_toggle_status(config)
		fmt.println("stoin: press Ctrl+C in this terminal to quit")

		tx_bolt: Tx_Bolt
		serial: Platform_Serial_Port
		connected := false
		announced_disconnected := false
		last_byte_ms: u64

		for {
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

cli_write :: proc(text: string) {
	_, _ = os.write_string(os.stdout, text)
}

cli_write_line :: proc(text: string) {
	cli_write(text)
	cli_write("\n")
}
