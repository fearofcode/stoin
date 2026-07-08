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

cli_runtime_write_suggestion :: proc(line: string, userdata: rawptr) -> bool {
	_ = userdata
	cli_write(line)
	return true
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

cli_write :: proc(text: string) {
	_, _ = os.write_string(os.stdout, text)
}

cli_write_line :: proc(text: string) {
	cli_write(text)
	cli_write("\n")
}
