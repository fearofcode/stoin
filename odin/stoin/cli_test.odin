package stoin

import "core:os"
import "core:testing"

@(test)
test_parse_cli_args_help :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--help"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Help)
}

@(test)
test_parse_cli_args_lookup :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Lookup)
	testing.expect_value(t, len(config.dict_paths), 1)
	testing.expect_value(t, config.dict_paths[0], "tests/test-dictionary.json")
	testing.expect_value(t, len(config.lookups), 1)
	testing.expect_value(t, config.lookups[0], "SA-P")
}

@(test)
test_parse_cli_args_dump_dictionary :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--dump-dictionary", "build/odin-dump.json"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Dump_Dictionary)
	testing.expect(t, config.dump_dictionary)
	testing.expect_value(t, config.dump_path, "build/odin-dump.json")
}

@(test)
test_parse_cli_args_config_file :: proc(t: ^testing.T) {
	mkdir_err := os.make_directory("build")
	testing.expect(t, mkdir_err == nil || mkdir_err == .Exist)

	path := "build/odin-cli-config.json"
	defer os.remove(path)
	testing.expect(t, runtime_test_write_file(path, `{
  "word_list": "tests/test-words.txt",
  "phrasing": "tests/test-phrasing.json",
  "dictionaries": [
    "tests/test-dictionary.json",
    {"path": "tests/test-modal-dictionary.json", "enabled": false},
    "tests/test-custom-dictionary.json"
  ]
}`))

	args := [?]string{APP_NAME, "--config", path, "--lookup", "KAT"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Lookup)
	testing.expect_value(t, config.orthography_path, "tests/test-words.txt")
	testing.expect_value(t, config.phrasing_path, "tests/test-phrasing.json")
	testing.expect_value(t, len(config.dict_paths), 3)
	testing.expect_value(t, config.dict_paths[0], "tests/test-dictionary.json")
	testing.expect_value(t, config.dict_paths[1], "tests/test-modal-dictionary.json")
	testing.expect_value(t, config.dict_paths[2], "tests/test-custom-dictionary.json")
	testing.expect_value(t, config.dict_enabled[0], true)
	testing.expect_value(t, config.dict_enabled[1], false)
	testing.expect_value(t, config.dict_enabled[2], true)
}

@(test)
test_parse_cli_args_dictionary_overrides_config_dictionaries :: proc(t: ^testing.T) {
	mkdir_err := os.make_directory("build")
	testing.expect(t, mkdir_err == nil || mkdir_err == .Exist)

	path := "build/odin-cli-config-override.json"
	defer os.remove(path)
	testing.expect(t, runtime_test_write_file(path, `{
  "dictionaries": [
    "tests/test-dictionary.json",
    {"path": "tests/test-modal-dictionary.json", "enabled": false}
  ]
}`))

	args := [?]string {
		APP_NAME,
		"--config", path,
		"--dictionary", "tests/test-custom-dictionary.json",
		"--lookup", "KAT",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, len(config.dict_paths), 1)
	testing.expect_value(t, config.dict_paths[0], "tests/test-custom-dictionary.json")
	testing.expect_value(t, len(config.dict_enabled), 1)
	testing.expect_value(t, config.dict_enabled[0], true)
}

@(test)
test_parse_cli_args_translate_consumes_remaining_outlines :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--translate", "KWEUBG", "-L"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect_value(t, len(config.dict_paths), 1)
	testing.expect_value(t, len(config.translates), 2)
	testing.expect_value(t, config.translates[0], "KWEUBG")
	testing.expect_value(t, config.translates[1], "-L")
}

@(test)
test_parse_cli_args_print_suggestions :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--print-suggestions", "--translate", "TPH", "-T"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect(t, config.print_suggestions)
	testing.expect_value(t, len(config.translates), 2)
}

@(test)
test_parse_cli_args_suggestion_log :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--suggestion-log", "suggestions.jsonl", "--translate", "TPH", "-T"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect_value(t, config.suggestion_log_path, "suggestions.jsonl")
	testing.expect_value(t, len(config.translates), 2)
}

@(test)
test_parse_cli_args_trace_strokes_default :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.trace_strokes)
}

@(test)
test_parse_cli_args_no_trace_strokes :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json", "--no-trace-strokes"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, !config.trace_strokes)
}

@(test)
test_parse_cli_args_trace_key_events :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json", "--trace-key-events"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.trace_key_events)
}

@(test)
test_parse_cli_args_trace_input_events_alias :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json", "--trace-input-events"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.trace_key_events)
}

@(test)
test_parse_cli_args_time_translations :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json", "--time-translations"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.time_translations)
}

@(test)
test_parse_cli_args_time_translation_alias :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "tx-bolt", "--dict", "tests/test-dictionary.json", "--time-translation"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.time_translations)
}

@(test)
test_parse_cli_args_qwerty_defaults_keymap :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Qwerty)
	testing.expect(t, config.input_qwerty)
	testing.expect_value(t, len(config.dict_paths), 1)
	testing.expect_value(t, config.keymap_path, "stoin.keymap")
}

@(test)
test_parse_cli_args_qwerty_custom_keymap :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--keymap", "tests/test.keymap"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Qwerty)
	testing.expect_value(t, config.keymap_path, "tests/test.keymap")
}

@(test)
test_parse_cli_args_tx_bolt :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "tx-bolt",
		"--dict", "tests/test-dictionary.json",
		"--serial-port", "/dev/cu.usbserial-test",
		"--serial-baud", "9600",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Tx_Bolt)
	testing.expect(t, config.input_tx_bolt)
	testing.expect_value(t, config.serial_port_path, "/dev/cu.usbserial-test")
	testing.expect_value(t, config.serial_baud_rate, 9600)
}

@(test)
test_parse_cli_args_tx_bolt_multiple_inputs :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "tx-bolt",
		"--dict", "tests/test-dictionary.json",
		"--multiple-inputs",
		"--multi-input-window-ms", "75",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Tx_Bolt)
	testing.expect(t, config.multiple_inputs)
	testing.expect_value(t, config.multi_input_window_ms, uint(75))
}

@(test)
test_parse_cli_args_tx_bolt_multiple_inputs_allows_zero_window :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "tx-bolt",
		"--dict", "tests/test-dictionary.json",
		"--multiple-inputs",
		"--multi-input-window-ms", "0",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.multi_input_window_ms, uint(0))
}

@(test)
test_parse_cli_args_gemini_pr :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "gemini",
		"--dict", "tests/test-dictionary.json",
		"--gemini-port", "/dev/cu.usbserial-gemini",
		"--gemini-baud", "9600",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Gemini_Pr)
	testing.expect(t, config.input_gemini_pr)
	testing.expect_value(t, config.serial_port_path, "/dev/cu.usbserial-gemini")
	testing.expect_value(t, config.serial_baud_rate, 9600)
}

@(test)
test_parse_cli_args_stentura :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "stenograph-8000",
		"--dict", "tests/test-dictionary.json",
		"--stentura-port", "/dev/cu.KeySerial1",
		"--stentura-baud", "9600",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Stentura)
	testing.expect(t, config.input_stentura)
	testing.expect_value(t, config.serial_port_path, "/dev/cu.KeySerial1")
	testing.expect_value(t, config.serial_baud_rate, 9600)
}

@(test)
test_parse_cli_args_stentura_aliases :: proc(t: ^testing.T) {
	input_modes := [?]string{"stentura", "stentura-8000", "stenograph", "stenograph-8000", "8000"}
	for input_mode in input_modes {
		args := [?]string{APP_NAME, "--input", input_mode, "--dict", "tests/test-dictionary.json"}
		config, ok := parse_cli_args(args[:])

		testing.expect(t, ok)
		testing.expect_value(t, config.mode, Cli_Mode.Stentura)
		testing.expect(t, config.input_stentura)
		cli_config_destroy(&config)
	}
}

@(test)
test_parse_cli_args_raw_serial :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--raw-serial",
		"--serial-port", "/dev/cu.usbserial-test",
		"--serial-baud", "9600",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Raw_Serial)
	testing.expect(t, config.raw_serial)
	testing.expect_value(t, config.serial_port_path, "/dev/cu.usbserial-test")
	testing.expect_value(t, config.serial_baud_rate, 9600)
}

@(test)
test_parse_cli_args_orthography :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--orthography", "tests/test-words.txt", "--translate", "STOER", "-Z"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect_value(t, config.orthography_path, "tests/test-words.txt")
	testing.expect_value(t, len(config.translates), 2)
}

@(test)
test_parse_cli_args_phrase_mode :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--dict", "tests/test-dictionary.json",
		"--phrasing", "tests/test-phrasing.json",
		"--phrase-mode", "verbs",
		"--translate", "PW-B",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect(t, config.phrase_mode_enabled)
	testing.expect_value(t, config.phrasing_path, "tests/test-phrasing.json")
	testing.expect_value(t, config.phrase_mode, Phrase_Lookup_Mode.Verbs)
}

@(test)
test_parse_cli_args_phrase_toggles :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "tx-bolt",
		"--dict", "tests/test-dictionary.json",
		"--phrasing", "tests/test-phrasing.json",
		"--phrase-toggle", "F13",
		"--nonverb-phrase-toggle", "F14",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Tx_Bolt)
	testing.expect(t, config.phrase_toggle_enabled)
	testing.expect(t, config.nonverb_phrase_toggle_enabled)
	testing.expect_value(t, config.phrase_toggle_keycode, u16(105))
	testing.expect_value(t, config.nonverb_phrase_toggle_keycode, u16(107))
}

@(test)
test_parse_cli_args_lookup_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Lookup)
	testing.expect_value(t, config.orthography_path, DEFAULT_WORD_LIST_PATH)
	testing.expect_value(t, config.phrasing_path, DEFAULT_PHRASING_PATH)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_translate_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--translate", "KWEUBG", "-L"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Translate)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_qwerty_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Qwerty)
	testing.expect_value(t, config.keymap_path, "stoin.keymap")
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_tx_bolt_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "bolt"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Tx_Bolt)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_gemini_pr_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "gemini-pr"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Gemini_Pr)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_stentura_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "stentura"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Stentura)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_rejects_combined_modes :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup, --dump-dictionary, --translate, --input, and --raw-serial cannot be combined")
}

@(test)
test_parse_cli_args_rejects_raw_serial_combined_with_translate :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--raw-serial", "--dict", "tests/test-dictionary.json", "--translate", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup, --dump-dictionary, --translate, --input, and --raw-serial cannot be combined")
}

@(test)
test_parse_cli_args_dictionary_alone_requires_mode :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--dictionary requires --lookup, --translate, --dump-dictionary, or --input")
}

@(test)
test_parse_cli_args_dump_dictionary_uses_default_config :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dump-dictionary"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect_value(t, config.mode, Cli_Mode.Dump_Dictionary)
	testing.expect(t, len(config.dict_paths) > 0)
}

@(test)
test_parse_cli_args_phrase_mode_uses_default_phrasing :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--phrase-mode", "verbs", "--translate", "PW-B"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.phrase_mode_enabled)
	testing.expect_value(t, config.phrasing_path, DEFAULT_PHRASING_PATH)
}

@(test)
test_parse_cli_args_phrase_toggle_uses_default_phrasing :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--phrase-toggle", "F13"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, ok)
	testing.expect(t, config.phrase_toggle_enabled)
	testing.expect_value(t, config.phrasing_path, DEFAULT_PHRASING_PATH)
}

@(test)
test_parse_cli_args_phrase_toggles_must_be_distinct :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "qwerty",
		"--dict", "tests/test-dictionary.json",
		"--phrasing", "tests/test-phrasing.json",
		"--phrase-toggle", "F13",
		"--nonverb-phrase-toggle", "F13",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--phrase-toggle and --nonverb-phrase-toggle must use distinct keys")
}

@(test)
test_parse_cli_args_rejects_multiple_inputs_without_tx_bolt :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "gemini-pr",
		"--dict", "tests/test-dictionary.json",
		"--multiple-inputs",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--multiple-inputs currently only supports --input tx-bolt")
}

@(test)
test_parse_cli_args_rejects_invalid_multi_input_window :: proc(t: ^testing.T) {
	args := [?]string {
		APP_NAME,
		"--input", "tx-bolt",
		"--dict", "tests/test-dictionary.json",
		"--multiple-inputs",
		"--multi-input-window-ms", "60001",
	}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--multi-input-window-ms requires 0..60000 milliseconds")
}
