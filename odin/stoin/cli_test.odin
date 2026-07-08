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
test_parse_cli_args_rejects_combined_modes :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--input", "qwerty", "--dict", "tests/test-dictionary.json", "--lookup", "SA-P"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--lookup, --translate, and --input cannot be combined")
}

@(test)
test_parse_cli_args_phrase_mode_requires_phrasing :: proc(t: ^testing.T) {
	args := [?]string{APP_NAME, "--dict", "tests/test-dictionary.json", "--phrase-mode", "verbs", "--translate", "PW-B"}
	config, ok := parse_cli_args(args[:])
	defer cli_config_destroy(&config)

	testing.expect(t, !ok)
	testing.expect_value(t, config.error_message, "--phrase-mode requires --phrasing")
}
