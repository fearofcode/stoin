package stoin

import "core:encoding/json"
import "core:os"

cli_config_clear_dictionary_paths :: proc(config: ^Cli_Config) {
	if config == nil {
		return
	}
	for path in config.dict_paths {
		owned_string_delete(path)
	}
	clear(&config.dict_paths)
	clear(&config.dict_enabled)
}

cli_config_add_dictionary_path :: proc(config: ^Cli_Config, path: string, enabled: bool) -> bool {
	if config == nil || len(path) == 0 {
		return false
	}
	path_copy, path_ok := clone_string_ok(path)
	if !path_ok {
		return false
	}
	append(&config.dict_paths, path_copy)
	append(&config.dict_enabled, enabled)
	return true
}

cli_config_set_orthography_path :: proc(config: ^Cli_Config, path: string) -> bool {
	if config == nil {
		return false
	}
	path_copy, path_ok := clone_string_ok(path)
	if !path_ok {
		return false
	}
	owned_string_delete(config.orthography_path)
	config.orthography_path = path_copy
	return true
}

cli_config_set_phrasing_path :: proc(config: ^Cli_Config, path: string) -> bool {
	if config == nil {
		return false
	}
	path_copy, path_ok := clone_string_ok(path)
	if !path_ok {
		return false
	}
	owned_string_delete(config.phrasing_path)
	config.phrasing_path = path_copy
	return true
}

cli_config_json_string :: proc(
	config: ^Cli_Config,
	object: json.Object,
	field: string,
	setter: proc(config: ^Cli_Config, path: string) -> bool,
) -> bool {
	value, found := object[field]
	if !found {
		return true
	}
	text, ok := value.(json.String)
	if !ok {
		config.error_message = "config field must be a string"
		return false
	}
	if !setter(config, string(text)) {
		config.error_message = "failed to store config path"
		return false
	}
	return true
}

cli_config_parse_dictionary_object :: proc(config: ^Cli_Config, item: json.Object) -> bool {
	path_value, path_found := item["path"]
	if !path_found {
		config.error_message = "config dictionary path must be a string"
		return false
	}
	path, path_ok := path_value.(json.String)
	if !path_ok {
		config.error_message = "config dictionary path must be a string"
		return false
	}

	enabled := true
	if enabled_value, enabled_found := item["enabled"]; enabled_found {
		enabled_bool, enabled_ok := enabled_value.(json.Boolean)
		if !enabled_ok {
			config.error_message = "config dictionary enabled must be true or false"
			return false
		}
		enabled = bool(enabled_bool)
	}

	if !cli_config_add_dictionary_path(config, string(path), enabled) {
		config.error_message = "failed to store config dictionary path"
		return false
	}
	return true
}

cli_config_parse_dictionaries :: proc(config: ^Cli_Config, object: json.Object) -> bool {
	value, found := object["dictionaries"]
	if !found {
		return true
	}
	array, ok := value.(json.Array)
	if !ok {
		config.error_message = "config dictionaries must be an array"
		return false
	}

	cli_config_clear_dictionary_paths(config)
	for item in array {
		if path, path_ok := item.(json.String); path_ok {
			if !cli_config_add_dictionary_path(config, string(path), true) {
				config.error_message = "failed to store config dictionary path"
				return false
			}
			continue
		}
		if dictionary_object, object_ok := item.(json.Object); object_ok {
			if !cli_config_parse_dictionary_object(config, dictionary_object) {
				return false
			}
			continue
		}
		config.error_message = "config dictionaries entries must be strings or objects"
		return false
	}
	return true
}

cli_config_load_runtime_config :: proc(config: ^Cli_Config, path: string) -> bool {
	if config == nil || len(path) == 0 {
		return false
	}

	data, read_err := os.read_entire_file(path, context.allocator)
	if read_err != nil {
		config.error_message = "failed to read config"
		return false
	}
	defer delete(data)

	root, parse_err := json.parse(data)
	if parse_err != nil {
		config.error_message = "config has invalid JSON"
		return false
	}
	defer json.destroy_value(root)

	object, ok := root.(json.Object)
	if !ok {
		config.error_message = "config must be a JSON object"
		return false
	}

	if !cli_config_json_string(config, object, "word_list", cli_config_set_orthography_path) ||
	   !cli_config_json_string(config, object, "phrasing", cli_config_set_phrasing_path) ||
	   !cli_config_parse_dictionaries(config, object) {
		return false
	}

	return true
}
