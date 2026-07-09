package stoin

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
test_parse_cli_args_requires_dictionary_for_lookup :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup requires at least one --dict")
}

@(test)
test_parse_cli_args_requires_dictionary_for_translate :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--translate", "KWEUBG", "-L"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--translate requires at least one --dict")
}

@(test)
test_parse_cli_args_requires_dictionary_for_qwerty :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--input qwerty requires at least one --dict")
}

@(test)
test_parse_cli_args_requires_dictionary_for_tx_bolt :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "bolt"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--input tx-bolt requires at least one --dict")
}

@(test)
test_parse_cli_args_requires_dictionary_for_gemini_pr :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "gemini-pr"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--input gemini-pr requires at least one --dict")
}

@(test)
test_parse_cli_args_rejects_combined_modes :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup, --translate, --input, and --raw-serial cannot be combined")
}

@(test)
test_parse_cli_args_rejects_raw_serial_combined_with_translate :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--raw-serial", "--dict", "tests/test-dictionary.json", "--translate", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup, --translate, --input, and --raw-serial cannot be combined")
}

@(test)
test_parse_cli_args_phrase_mode_requires_phrasing :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--phrase-mode", "verbs", "--translate", "PW-B"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--phrase-mode requires --phrasing")
}

@(test)
test_parse_cli_args_phrase_toggle_requires_phrasing :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--phrase-toggle", "F13"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--phrase-toggle requires --phrasing")
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
