package stoin

import "core:fmt"
import "core:os"

APP_NAME :: "stoin"

Cli_Mode :: enum {
	Scaffold,
	Help,
	Lookup,
	Translate,
}

Cli_Config :: struct {
	mode:          Cli_Mode,
	dict_paths:    [dynamic]string,
	lookups:       [dynamic]string,
	translates:     [dynamic]string,
	print_suggestions: bool,
	suggestion_log_path: string,
	error_message: string,
}

parse_cli_args :: proc(args: []string) -> (config: Cli_Config, ok: bool) {
	config.mode = .Scaffold
	config.dict_paths = make([dynamic]string)
	config.lookups = make([dynamic]string)
	config.translates = make([dynamic]string)

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
		case "--print-suggestions":
			config.print_suggestions = true
		case "--suggestion-log":
			if i + 1 >= len(args) {
				config.error_message = "--suggestion-log requires a path"
				return config, false
			}
			config.suggestion_log_path = args[i + 1]
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

	if len(config.lookups) > 0 && len(config.translates) > 0 {
		config.error_message = "--lookup and --translate cannot be combined"
		return config, false
	}

	if len(config.lookups) > 0 {
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
	} else if len(config.dict_paths) > 0 {
		config.error_message = "--dict requires --lookup or --translate"
		return config, false
	}

	return config, true
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

run_translate_cli :: proc(config: ^Cli_Config) -> bool {
	dictionary: Dictionary
	dictionary_init(&dictionary)
	defer dictionary_destroy(&dictionary)

	if !dictionary_load_many(&dictionary, config.dict_paths[:]) {
		fmt.eprintln("stoin: failed to load dictionary")
		return false
	}

	engine: Simple_Engine
	simple_engine_init(&engine, &dictionary)
	defer simple_engine_destroy(&engine)

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

	for outline in config.translates {
		bits, parsed := stroke_string_to_bits(outline)
		if !parsed || !simple_engine_translate_bits(&engine, bits) {
			fmt.eprintln("stoin: failed to translate outline sequence")
			return false
		}
		if config.print_suggestions || suggestion_log_file != nil {
			suggestion, found := brevity_suggest(&engine)
			if found {
				if suggestion_log_file != nil && !brevity_log_suggestion(suggestion_log_file, &suggestion) {
					brevity_suggestion_destroy(&suggestion)
					fmt.eprintln("stoin: failed to write suggestion log")
					return false
				}
				if config.print_suggestions {
					cli_write("Suggestion: Use ")
					cli_write(suggestion.suggested_outline)
					cli_write(" for \"")
					cli_write(suggestion.text)
					cli_write_line("\"")
				}
				brevity_suggestion_destroy(&suggestion)
			}
		}
	}

	text, ok := simple_engine_render(&engine)
	if !ok {
		fmt.eprintln("stoin: failed to render outline sequence")
		return false
	}
	defer owned_string_delete(text)

	for outline, i in config.translates {
		if i != 0 {
			cli_write("/")
		}
		cli_write(outline)
	}
	cli_write(" -> ")
	cli_write_line(text)
	return true
}

cli_write :: proc(text: string) {
	_, _ = os.write_string(os.stdout, text)
}

cli_write_line :: proc(text: string) {
	cli_write(text)
	cli_write("\n")
}
