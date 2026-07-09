package stoin

import "core:fmt"
import "core:os"

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
		dictionary_enabled = config.dict_enabled[:],
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
	when ODIN_OS != .Darwin && ODIN_OS != .Linux && ODIN_OS != .Windows {
		_ = config
		fmt.eprintln("stoin: qwerty input is currently implemented only on macOS, Linux, and Windows")
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
			keymap_path = config.keymap_path,
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
		if !steno_runtime_owner_init(&owner, &runtime_config) {
			fmt.eprintln("stoin: failed to initialize runtime")
			return false
		}
		defer steno_runtime_owner_destroy(&owner)

		if config.phrase_mode_enabled {
			steno_runtime_set_phrase_namespace_enabled(&owner.runtime, true)
			steno_runtime_set_phrase_mode(&owner.runtime, steno_phrase_mode_from_lookup_mode(config.phrase_mode))
		}
		cli_configure_runtime_input_options(config, &owner.runtime)

		if !cli_platform_qwerty_start(&owner) {
			fmt.eprintln("stoin: failed to start", cli_platform_qwerty_name())
			return false
		}
		cli_platform_translation_timing_set_enabled(config.time_translations)
		defer cli_platform_output_shutdown()
		defer cli_platform_translation_timing_set_enabled(false)
		defer cli_platform_qwerty_stop()

		fmt.eprintln("stoin:", cli_platform_qwerty_name(), "running")
		fmt.eprintln(
			"stoin: loaded",
			keymap_binding_count(&owner.keymap),
			"key bindings and",
			dictionary_count(&owner.dictionary_stack.dictionary),
			"dictionary entries",
		)
		cli_print_phrase_toggle_status(config)
		cli_print_translation_timing_status(config)
		fmt.eprintln("stoin: press Ctrl+Esc to toggle capture; press Ctrl+C in this terminal to quit")
		cli_platform_qwerty_run()
		return true
	}
}

