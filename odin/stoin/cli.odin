package stoin

import "core:fmt"

APP_NAME :: "stoin"

Cli_Mode :: enum {
	Scaffold,
	Help,
	Lookup,
}

Cli_Config :: struct {
	mode:          Cli_Mode,
	dict_paths:    [dynamic]string,
	lookups:       [dynamic]string,
	error_message: string,
}

parse_cli_args :: proc(args: []string) -> (config: Cli_Config, ok: bool) {
	config.mode = .Scaffold
	config.dict_paths = make([dynamic]string)
	config.lookups = make([dynamic]string)

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
		case:
			config.error_message = "unknown argument"
			return config, false
		}
	}

	if len(config.lookups) > 0 {
		config.mode = .Lookup
		if len(config.dict_paths) == 0 {
			config.error_message = "--lookup requires at least one --dict"
			return config, false
		}
	} else if len(config.dict_paths) > 0 {
		config.error_message = "--dict requires at least one --lookup"
		return config, false
	}

	return config, true
}

cli_config_destroy :: proc(config: ^Cli_Config) {
	delete(config.dict_paths)
	delete(config.lookups)
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
			fmt.printfln("%s -> %s", outline, translation)
		} else {
			fmt.eprintfln("stoin: no exact dictionary entry for %s", outline)
			all_ok = false
		}
	}
	return all_ok
}
