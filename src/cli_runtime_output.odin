package stoin

import "core:os"

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

cli_platform_live_output_supported :: proc() -> bool {
	when ODIN_OS == .Darwin || ODIN_OS == .Linux || ODIN_OS == .Windows {
		return true
	} else {
		return false
	}
}

cli_platform_live_output_name :: proc() -> string {
	when ODIN_OS == .Darwin {
		return "macOS text output"
	} else when ODIN_OS == .Linux {
		return "Linux uinput text output"
	} else when ODIN_OS == .Windows {
		return "Windows SendInput text output"
	} else {
		return "platform text output"
	}
}

cli_platform_configure_runtime_output :: proc(runtime_config: ^Steno_Runtime_Load_Config, config: ^Cli_Config) -> bool {
	if runtime_config == nil {
		return false
	}
	when ODIN_OS == .Darwin {
		runtime_config.send_text = macos_runtime_send_text
		runtime_config.delete_text = macos_runtime_delete_text
		runtime_config.send_key_combination = macos_runtime_send_key_combination
		if config != nil && config.time_translations {
			runtime_config.begin_translation_timing = macos_translation_timing_begin
			runtime_config.cancel_translation_timing = macos_translation_timing_cancel
		}
		return true
	} else when ODIN_OS == .Linux {
		runtime_config.send_text = linux_runtime_send_text
		runtime_config.delete_text = linux_runtime_delete_text
		runtime_config.send_key_combination = linux_runtime_send_key_combination
		if config != nil && config.time_translations {
			runtime_config.begin_translation_timing = linux_translation_timing_begin
			runtime_config.cancel_translation_timing = linux_translation_timing_cancel
		}
		return true
	} else when ODIN_OS == .Windows {
		runtime_config.send_text = windows_runtime_send_text
		runtime_config.delete_text = windows_runtime_delete_text
		runtime_config.send_key_combination = windows_runtime_send_key_combination
		if config != nil && config.time_translations {
			runtime_config.begin_translation_timing = windows_translation_timing_begin
			runtime_config.cancel_translation_timing = windows_translation_timing_cancel
		}
		return true
	} else {
		return false
	}
}

cli_platform_output_init :: proc() -> bool {
	when ODIN_OS == .Darwin {
		return macos_output_init()
	} else when ODIN_OS == .Linux {
		return linux_output_init()
	} else when ODIN_OS == .Windows {
		return windows_output_init()
	} else {
		return false
	}
}

cli_platform_output_shutdown :: proc() {
	when ODIN_OS == .Darwin {
		macos_output_shutdown()
	} else when ODIN_OS == .Linux {
		linux_output_shutdown()
	} else when ODIN_OS == .Windows {
		windows_output_shutdown()
	}
}

cli_platform_translation_timing_set_enabled :: proc(enabled: bool) {
	when ODIN_OS == .Darwin {
		macos_translation_timing_set_enabled(enabled)
	} else when ODIN_OS == .Linux {
		linux_translation_timing_set_enabled(enabled)
	} else when ODIN_OS == .Windows {
		windows_translation_timing_set_enabled(enabled)
	} else {
		_ = enabled
	}
}

cli_platform_keyboard_listener_supported :: proc() -> bool {
	when ODIN_OS == .Darwin || ODIN_OS == .Linux || ODIN_OS == .Windows {
		return true
	} else {
		return false
	}
}

cli_platform_keyboard_listener_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	when ODIN_OS == .Darwin {
		return macos_keyboard_listen_start(owner)
	} else when ODIN_OS == .Linux {
		return linux_keyboard_listen_start(owner)
	} else when ODIN_OS == .Windows {
		return windows_keyboard_listen_start(owner)
	} else {
		_ = owner
		return false
	}
}

cli_platform_keyboard_listener_stop :: proc() {
	when ODIN_OS == .Darwin {
		macos_keyboard_listen_stop()
	} else when ODIN_OS == .Linux {
		linux_keyboard_listen_stop()
	} else when ODIN_OS == .Windows {
		windows_keyboard_listen_stop()
	}
}

cli_platform_qwerty_name :: proc() -> string {
	when ODIN_OS == .Darwin {
		return "macOS Odin qwerty event tap"
	} else when ODIN_OS == .Linux {
		return "Linux Odin qwerty evdev capture"
	} else when ODIN_OS == .Windows {
		return "Windows Odin qwerty keyboard hook"
	} else {
		return "platform qwerty capture"
	}
}

cli_platform_qwerty_start :: proc(owner: ^Steno_Runtime_Owner) -> bool {
	when ODIN_OS == .Darwin {
		return macos_qwerty_start(owner)
	} else when ODIN_OS == .Linux {
		return linux_qwerty_start(owner)
	} else when ODIN_OS == .Windows {
		return windows_qwerty_start(owner)
	} else {
		_ = owner
		return false
	}
}

cli_platform_qwerty_run :: proc() {
	when ODIN_OS == .Darwin {
		macos_qwerty_run()
	} else when ODIN_OS == .Linux {
		linux_qwerty_run()
	} else when ODIN_OS == .Windows {
		windows_qwerty_run()
	}
}

cli_platform_qwerty_stop :: proc() {
	when ODIN_OS == .Darwin {
		macos_qwerty_stop()
	} else when ODIN_OS == .Linux {
		linux_qwerty_stop()
	} else when ODIN_OS == .Windows {
		windows_qwerty_stop()
	}
}

