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
