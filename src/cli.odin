package stoin

import "core:fmt"
import "core:os"

APP_NAME :: "stoin"
DEFAULT_CONFIG_PATH :: "stoin-config.json"
DEFAULT_DICTIONARY_PATH :: "lapwing-base.json"
DEFAULT_WORD_LIST_PATH :: "american_english_words.txt"
DEFAULT_PHRASING_PATH :: "phrasing.json"
INPUT_EVENT_POLL_SLEEP_MS :: 10
TX_BOLT_STROKE_IDLE_FLUSH_MS :: 100
TX_BOLT_MULTIPLE_DEFAULT_WINDOW_MS :: 150
TX_BOLT_MULTIPLE_MAX_DEVICES :: 16
TX_BOLT_MULTIPLE_SCAN_INTERVAL_MS :: 1000
TX_BOLT_MULTIPLE_LOOP_SLEEP_MS :: 1
RELOAD_POLL_INTERVAL_MS :: 250
RAW_SERIAL_BURST_CAPACITY :: 256

Cli_Mode :: enum {
	Scaffold,
	Help,
	Lookup,
	Dump_Dictionary,
	Translate,
	Qwerty,
	Tx_Bolt,
	Gemini_Pr,
	Stentura,
	Raw_Serial,
}

Cli_Config :: struct {
	mode:          Cli_Mode,
	dict_paths:    [dynamic]string,
	dict_enabled:  [dynamic]bool,
	dump_dictionary: bool,
	dump_path:     string,
	lookups:       [dynamic]string,
	translates:     [dynamic]string,
	input_qwerty:   bool,
	input_tx_bolt:  bool,
	input_gemini_pr: bool,
	input_stentura: bool,
	raw_serial:     bool,
	multiple_inputs: bool,
	multi_input_window_ms: uint,
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
	trace_strokes: bool,
	trace_key_events: bool,
	time_translations: bool,
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
	config.dict_enabled = make([dynamic]bool)
	config.lookups = make([dynamic]string)
	config.translates = make([dynamic]string)
	config.phrase_mode = .All
	config.serial_baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
	config.multi_input_window_ms = TX_BOLT_MULTIPLE_DEFAULT_WINDOW_MS
	config.trace_strokes = true

	raw_serial_requested := false
	for i := 1; i < len(args); i += 1 {
	if args[i] == "--raw-serial" || args[i] == "--dump-serial" {
			raw_serial_requested = true
		}
	}
	if !raw_serial_requested {
		if !cli_config_set_orthography_path(&config, DEFAULT_WORD_LIST_PATH) ||
		   !cli_config_set_phrasing_path(&config, DEFAULT_PHRASING_PATH) {
			config.error_message = "failed to store default config paths"
			return config, false
		}

		config_path := DEFAULT_CONFIG_PATH
		config_path_explicit := false
		for i := 1; i < len(args); i += 1 {
			if args[i] == "--config" {
				if i + 1 >= len(args) {
					break
				}
				config_path = args[i + 1]
				config_path_explicit = true
				i += 1
			}
		}
		if !cli_config_load_runtime_config(&config, config_path, !config_path_explicit) {
			return config, false
		}
	}

	cli_dictionary_paths := false
	dictionary_argument_seen := false
	for i := 1; i < len(args); i += 1 {
		arg := args[i]
		switch arg {
		case "--help", "-h":
			config.mode = .Help
		case "--config":
			if i + 1 >= len(args) {
				config.error_message = "--config requires a path"
				return config, false
			}
			i += 1
		case "--dict", "--dictionary":
			if i + 1 >= len(args) {
				config.error_message = "--dictionary requires a path"
				return config, false
			}
			if !cli_dictionary_paths {
				cli_config_clear_dictionary_paths(&config)
				cli_dictionary_paths = true
			}
			dictionary_argument_seen = true
			if !cli_config_add_dictionary_path(&config, args[i + 1], true) {
				config.error_message = "failed to store dictionary path"
				return config, false
			}
			i += 1
		case "--lookup":
			if i + 1 >= len(args) {
				config.error_message = "--lookup requires an outline"
				return config, false
			}
			append(&config.lookups, args[i + 1])
			i += 1
		case "--dump-dictionary":
			config.dump_dictionary = true
			if i + 1 < len(args) && (len(args[i + 1]) == 0 || args[i + 1][0] != '-') {
				config.dump_path = args[i + 1]
				i += 1
			}
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
			} else if input_mode == "gemini-pr" || input_mode == "gemini" {
				config.input_gemini_pr = true
			} else if input_mode == "stentura" ||
			          input_mode == "stentura-8000" ||
			          input_mode == "stenograph" ||
			          input_mode == "stenograph-8000" ||
			          input_mode == "8000" {
				config.input_stentura = true
			} else {
				config.error_message = "--input currently supports qwerty, tx-bolt, gemini-pr, or stentura"
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
		case "--serial-port", "--port", "--tx-bolt-port", "--gemini-port", "--stentura-port":
			if i + 1 >= len(args) {
				config.error_message = "--serial-port requires a path"
				return config, false
			}
			config.serial_port_path = args[i + 1]
			i += 1
		case "--serial-baud", "--baud", "--tx-bolt-baud", "--gemini-baud", "--stentura-baud":
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
		case "--multiple-inputs":
			config.multiple_inputs = true
		case "--multi-input-window-ms":
			if i + 1 >= len(args) {
				config.error_message = "--multi-input-window-ms requires milliseconds"
				return config, false
			}
			window_ms, window_ok := parse_cli_milliseconds(args[i + 1])
			if !window_ok {
				config.error_message = "--multi-input-window-ms requires 0..60000 milliseconds"
				return config, false
			}
			config.multi_input_window_ms = window_ms
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
		case "--trace-strokes":
			config.trace_strokes = true
		case "--no-trace-strokes":
			config.trace_strokes = false
		case "--trace-key-events", "--trace-input-events":
			config.trace_key_events = true
		case "--time-translations", "--time-translation":
			config.time_translations = true
		case "--word-list", "--orthography":
			if i + 1 >= len(args) {
				config.error_message = "--word-list requires a path"
				return config, false
			}
			if !cli_config_set_orthography_path(&config, args[i + 1]) {
				config.error_message = "failed to store word-list path"
				return config, false
			}
			i += 1
		case "--phrasing":
			if i + 1 >= len(args) {
				config.error_message = "--phrasing requires a path"
				return config, false
			}
			if !cli_config_set_phrasing_path(&config, args[i + 1]) {
				config.error_message = "failed to store phrasing path"
				return config, false
			}
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
	if config.dump_dictionary {
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
	if config.input_gemini_pr {
		selected_modes += 1
	}
	if config.input_stentura {
		selected_modes += 1
	}
	if config.raw_serial {
		selected_modes += 1
	}
	if selected_modes > 1 {
		config.error_message = "--lookup, --dump-dictionary, --translate, --input, and --raw-serial cannot be combined"
		return config, false
	}

	if !raw_serial_requested && len(config.dict_paths) == 0 {
		if !cli_config_add_dictionary_path(&config, DEFAULT_DICTIONARY_PATH, true) {
			config.error_message = "failed to store default dictionary path"
			return config, false
		}
	}

	if config.multiple_inputs && !config.input_tx_bolt {
		config.error_message = "--multiple-inputs currently only supports --input tx-bolt"
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
	} else if config.dump_dictionary {
		config.mode = .Dump_Dictionary
		if len(config.dict_paths) == 0 {
			config.error_message = "--dump-dictionary requires at least one --dict"
			return config, false
		}
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
	} else if config.input_gemini_pr {
		config.mode = .Gemini_Pr
		if len(config.dict_paths) == 0 {
			config.error_message = "--input gemini-pr requires at least one --dict"
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
	} else if config.input_stentura {
		config.mode = .Stentura
		if len(config.dict_paths) == 0 {
			config.error_message = "--input stentura requires at least one --dict"
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
	} else if len(config.translates) > 0 {
		config.mode = .Translate
		if config.phrase_mode_enabled && len(config.phrasing_path) == 0 {
			config.error_message = "--phrase-mode requires --phrasing"
			return config, false
		}
	} else if dictionary_argument_seen {
		config.error_message = "--dictionary requires --lookup, --translate, --dump-dictionary, or --input"
		return config, false
	}

	return config, true
}

parse_cli_milliseconds :: proc(text: string) -> (value: uint, ok: bool) {
	if len(text) == 0 {
		return 0, false
	}
	for i in 0..<len(text) {
		if text[i] < '0' || text[i] > '9' {
			return 0, false
		}
		value = value * 10 + uint(text[i] - '0')
		if value > 60000 {
			return 0, false
		}
	}
	return value, true
}

cli_phrase_toggles_enabled :: proc(config: ^Cli_Config) -> bool {
	return config != nil && (config.phrase_toggle_enabled || config.nonverb_phrase_toggle_enabled)
}

cli_keyboard_listener_enabled :: proc(config: ^Cli_Config) -> bool {
	return cli_phrase_toggles_enabled(config) || (config != nil && config.trace_key_events)
}

cli_configure_runtime_input_options :: proc(config: ^Cli_Config, runtime: ^Steno_Runtime) {
	cli_configure_phrase_toggles(config, runtime)
	if config != nil {
		steno_runtime_set_trace_key_events(runtime, config.trace_key_events)
	}
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

cli_print_translation_timing_status :: proc(config: ^Cli_Config) {
	if config == nil || !config.time_translations {
		return
	}
	fmt.eprintln("stoin: translation timing enabled; latency stops immediately before the first platform output event")
	if config.trace_strokes {
		fmt.eprintln("stoin: note: stroke tracing is enabled and included in the measured path; use --no-trace-strokes for cleaner benchmark numbers")
	}
}

cli_config_destroy :: proc(config: ^Cli_Config) {
	cli_config_clear_dictionary_paths(config)
	delete(config.dict_paths)
	delete(config.dict_enabled)
	delete(config.lookups)
	delete(config.translates)
	owned_string_delete(config.orthography_path)
	owned_string_delete(config.phrasing_path)
	config^ = {}
}

run_lookup_cli :: proc(config: ^Cli_Config) -> bool {
	stack: Dictionary_Stack
	dictionary_stack_init(&stack)
	defer dictionary_stack_destroy(&stack)

	if !dictionary_stack_set_paths(&stack, config.dict_paths[:], config.dict_enabled[:]) ||
	   !dictionary_stack_load(&stack) {
		fmt.eprintln("stoin: failed to load dictionary")
		return false
	}

	for outline in config.lookups {
		if translation, found := dictionary_lookup_stroke(&stack.dictionary, outline); found {
			cli_write(outline)
			cli_write(" -> ")
			cli_write_line(translation)
		} else {
			cli_write(outline)
			cli_write(" -> ")
			cli_write_line("[untranslated]")
		}
	}
	return true
}

run_dump_dictionary_cli :: proc(config: ^Cli_Config) -> bool {
	stack: Dictionary_Stack
	dictionary_stack_init(&stack)
	defer dictionary_stack_destroy(&stack)

	if !dictionary_stack_set_paths(&stack, config.dict_paths[:], config.dict_enabled[:]) ||
	   !dictionary_stack_load(&stack) {
		fmt.eprintln("stoin: failed to load dictionary")
		return false
	}

	path := config.dump_path
	if len(path) == 0 {
		path = config.dict_paths[len(config.dict_paths) - 1]
	}
	if !dictionary_dump_json(&stack.dictionary, path) {
		fmt.eprintln("stoin: failed to dump dictionary to", path)
		return false
	}
	fmt.println("stoin: wrote", dictionary_count(&stack.dictionary), "entries to", path)
	return true
}

cli_write :: proc(text: string) {
	_, _ = os.write_string(os.stdout, text)
}

cli_write_line :: proc(text: string) {
	cli_write(text)
	cli_write("\n")
}
