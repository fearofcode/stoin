package stoin

Dictionary_Stack :: struct {
	paths:      [dynamic]string,
	enabled:    [dynamic]bool,
	dictionary: Dictionary,
}

dictionary_stack_init :: proc(stack: ^Dictionary_Stack) {
	stack^ = {}
	stack.paths = make([dynamic]string)
	stack.enabled = make([dynamic]bool)
	dictionary_init(&stack.dictionary)
}

dictionary_stack_destroy_paths :: proc(stack: ^Dictionary_Stack) {
	for path in stack.paths {
		owned_string_delete(path)
	}
	delete(stack.paths)
	delete(stack.enabled)
	stack.paths = nil
	stack.enabled = nil
}

dictionary_stack_destroy :: proc(stack: ^Dictionary_Stack) {
	dictionary_stack_destroy_paths(stack)
	dictionary_destroy(&stack.dictionary)
	stack^ = {}
}

dictionary_stack_set_paths :: proc(stack: ^Dictionary_Stack, paths: []string, enabled: []bool) -> bool {
	dictionary_stack_destroy_paths(stack)
	stack.paths = make([dynamic]string)
	stack.enabled = make([dynamic]bool)

	if len(paths) == 0 {
		return false
	}

	for path, i in paths {
		if len(path) == 0 {
			dictionary_stack_destroy_paths(stack)
			return false
		}
		copy, ok := clone_string_ok(path)
		if !ok {
			dictionary_stack_destroy_paths(stack)
			return false
		}
		append(&stack.paths, copy)
		is_enabled := true
		if len(enabled) > i {
			is_enabled = enabled[i]
		}
		append(&stack.enabled, is_enabled)
	}
	return true
}

dictionary_stack_load :: proc(stack: ^Dictionary_Stack) -> bool {
	if len(stack.paths) == 0 || len(stack.paths) != len(stack.enabled) {
		return false
	}

	next: Dictionary
	dictionary_init(&next)
	for path, i in stack.paths {
		if !stack.enabled[i] {
			continue
		}
		if !dictionary_load(&next, path) {
			dictionary_destroy(&next)
			return false
		}
	}
	dictionary_destroy(&stack.dictionary)
	stack.dictionary = next
	return true
}

path_basename :: proc(path: string) -> string {
	start := 0
	for i in 0..<len(path) {
		if path[i] == '/' || path[i] == '\\' {
			start = i + 1
		}
	}
	return path[start:]
}

path_has_selection_suffix :: proc(path: string, selection: string) -> bool {
	if len(selection) == 0 || len(selection) > len(path) {
		return false
	}
	start := len(path) - len(selection)
	if path[start:] != selection {
		return false
	}
	return start == 0 || path[start - 1] == '/' || path[start - 1] == '\\'
}

dictionary_stack_path_matches_selection :: proc(path: string, selection: string) -> bool {
	if len(path) == 0 || len(selection) == 0 {
		return false
	}
	if path_has_selection_suffix(path, selection) {
		return true
	}

	base := path_basename(path)
	selection_base := path_basename(selection)
	if base == selection_base {
		return true
	}
	lapwing_prefix := "lapwing-"
	if len(base) > len(lapwing_prefix) && base[:len(lapwing_prefix)] == lapwing_prefix && base[len(lapwing_prefix):] == selection_base {
		return true
	}
	return false
}

dictionary_stack_toggle_selection :: proc(stack: ^Dictionary_Stack, toggle: byte, selection: string) -> bool {
	match := -1
	match_length := 0
	for path, i in stack.paths {
		if !dictionary_stack_path_matches_selection(path, selection) {
			continue
		}
		if match < 0 || len(path) < match_length {
			match = i
			match_length = len(path)
		}
	}

	if match < 0 {
		return true
	}

	old_enabled := stack.enabled[match]
	new_enabled := old_enabled
	switch toggle {
	case '+':
		new_enabled = true
	case '-':
		new_enabled = false
	case '!':
		new_enabled = !old_enabled
	case:
		return true
	}

	if new_enabled == old_enabled {
		return true
	}

	stack.enabled[match] = new_enabled
	if !dictionary_stack_load(stack) {
		stack.enabled[match] = old_enabled
		dictionary_stack_load(stack)
		return false
	}
	return true
}

trim_ascii_space :: proc(text: string) -> string {
	start := 0
	end := len(text)
	for start < end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\n' || text[start] == '\r') {
		start += 1
	}
	for end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' || text[end - 1] == '\r') {
		end -= 1
	}
	return text[start:end]
}

dictionary_stack_toggle :: proc(stack: ^Dictionary_Stack, selections: string) -> bool {
	if stack == nil {
		return false
	}

	start := 0
	for {
		end := start
		for end < len(selections) && selections[end] != ',' {
			end += 1
		}

		selection := trim_ascii_space(selections[start:end])
		if len(selection) > 0 {
			toggle := selection[0]
			path := selection[1:]
			if !dictionary_stack_toggle_selection(stack, toggle, path) {
				return false
			}
		}

		if end == len(selections) {
			break
		}
		start = end + 1
	}
	return true
}
