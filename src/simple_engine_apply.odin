package stoin

simple_engine_range_text :: proc(engine: ^Simple_Engine, start: int, count: int) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	for i in start..<start + count {
		formatted_append_string(&buffer, engine.history[i].text)
	}
	return clone_bytes_to_string(buffer[:])
}

applied_translation_source_text :: proc(translation: ^Applied_Translation) -> (string, bool) {
	if len(translation.replaced) == 0 {
		return clone_string_ok(translation.text)
	}

	buffer := make([dynamic]byte)
	defer delete(buffer)
	for i in 0..<len(translation.replaced) {
		source, source_ok := applied_translation_source_text(&translation.replaced[i])
		if !source_ok {
			return "", false
		}
		formatted_append_string(&buffer, source)
		owned_string_delete(source)
	}
	return clone_bytes_to_string(buffer[:])
}

simple_engine_range_source_text :: proc(engine: ^Simple_Engine, start: int, count: int) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	for i in start..<start + count {
		source, source_ok := applied_translation_source_text(&engine.history[i])
		if !source_ok {
			return "", false
		}
		formatted_append_string(&buffer, source)
		owned_string_delete(source)
	}
	return clone_bytes_to_string(buffer[:])
}

applied_translation_replaced_text :: proc(translation: ^Applied_Translation) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)
	for i in 0..<len(translation.replaced) {
		formatted_append_string(&buffer, translation.replaced[i].text)
	}
	return clone_bytes_to_string(buffer[:])
}

simple_engine_previous_visible :: proc(engine: ^Simple_Engine, before_index: int) -> (translation: ^Applied_Translation) {
	for i := before_index; i > 0; {
		i -= 1
		if len(engine.history[i].text) > 0 {
			return &engine.history[i]
		}
	}
	return nil
}

text_starts_with_prefix :: proc(text: string, prefix: string) -> bool {
	return len(prefix) > 0 && len(text) >= len(prefix) && text[:len(prefix)] == prefix
}

text_ends_with_suffix :: proc(text: string, suffix: string) -> bool {
	return len(suffix) > 0 && len(text) >= len(suffix) && text[len(text) - len(suffix):] == suffix
}

simple_engine_should_prepend_spacing :: proc(engine: ^Simple_Engine, previous: ^Applied_Translation, formatted: ^Formatted_Text) -> bool {
	if len(formatted.text) == 0 || formatted.attach_prev || formatted.glue || len(engine.spacing) == 0 || text_starts_with_prefix(formatted.text, engine.spacing) {
		return false
	}
	if previous == nil {
		return false
	}
	if previous.next_attach || text_ends_with_suffix(previous.text, engine.spacing) {
		return false
	}
	return true
}

simple_engine_build_text :: proc(engine: ^Simple_Engine, old_text: string, previous: ^Applied_Translation, formatted: ^Formatted_Text) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	if formatted.attach_prev {
		if len(formatted.ortho_suffix) > 0 {
			word_start, word_end := last_word_bounds(old_text)
			formatted_append_string(&buffer, old_text[:word_start])
			joined: string
			joined_ok: bool
			if engine.orthography != nil {
				joined, joined_ok = orthography_apply(engine.orthography, old_text[word_start:word_end], formatted.ortho_suffix)
			} else {
				joined, joined_ok = orthography_apply_basic(old_text[word_start:word_end], formatted.ortho_suffix)
			}
			if !joined_ok {
				return "", false
			}
			formatted_append_string(&buffer, formatted.text[:formatted.ortho_suffix_text_offset])
			formatted_append_string(&buffer, joined)
			owned_string_delete(joined)
			suffix_end := formatted.ortho_suffix_text_offset + formatted.ortho_suffix_text_length
			if suffix_end < len(formatted.text) {
				formatted_append_string(&buffer, formatted.text[suffix_end:])
			}
			formatted_append_string(&buffer, old_text[word_end:])
		} else {
			formatted_append_string(&buffer, old_text)
		}
	} else if simple_engine_should_prepend_spacing(engine, previous, formatted) {
		formatted_append_string(&buffer, engine.spacing)
	}
	if len(formatted.ortho_suffix) == 0 {
		formatted_append_string(&buffer, formatted.text)
	}
	return clone_bytes_to_string(buffer[:])
}

simple_engine_match_has_partial_prefix :: proc(match: ^Translation_Match) -> bool {
	return match.partial_prefix_stroke_count > 0
}

simple_engine_build_partial_replacement_text :: proc(engine: ^Simple_Engine, prefix_text: string, previous: ^Applied_Translation, formatted: ^Formatted_Text) -> (string, bool) {
	if formatted.attach_prev {
		return simple_engine_build_text(engine, prefix_text, previous, formatted)
	}

	synthetic_previous := Applied_Translation{text = prefix_text}
	previous_for_suffix := len(prefix_text) > 0 ? &synthetic_previous : previous
	suffix_text, suffix_ok := simple_engine_build_text(engine, "", previous_for_suffix, formatted)
	if !suffix_ok {
		return "", false
	}
	defer owned_string_delete(suffix_text)

	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, prefix_text)
	formatted_append_string(&buffer, suffix_text)
	return clone_bytes_to_string(buffer[:])
}

simple_engine_apply_match :: proc(engine: ^Simple_Engine, match: ^Translation_Match) -> bool {
	if match.suffix_match {
		return simple_engine_apply_suffix_match(engine, match)
	}

	formatted, formatted_ok := format_translation_text_basic(match.translation)
	if !formatted_ok {
		return false
	}
	defer formatted_text_destroy(&formatted)

	if formatted.retro_command != .None {
		return simple_engine_apply_retro_command(engine, match, formatted.retro_command)
	}

	if formatted.stitch_last_word {
		return simple_engine_apply_stitch_last_word(engine, match, &formatted)
	}

	if formatted.retro_case != .Normal {
		return simple_engine_apply_retro_case(engine, match, formatted.retro_case)
	}

	if len(formatted.mode_command) > 0 &&
	   len(formatted.text) == 0 &&
	   len(formatted.key_combos) == 0 &&
	   len(formatted.plover_command) == 0 &&
	   !formatted.attach_prev &&
	   !formatted.attach_next {
		return simple_engine_execute_mode_command(engine, formatted.mode_command)
	}

	if len(formatted.plover_command) > 0 &&
	   len(formatted.text) == 0 &&
	   len(formatted.key_combos) == 0 &&
	   !formatted.attach_prev &&
	   !formatted.attach_next {
		return simple_engine_execute_plover_command(engine, formatted.plover_command)
	}

	previous_case_mode := engine.case_mode
	previous_next_case := engine.next_case
	if !simple_engine_apply_case_state_to_formatted(engine, &formatted) {
		return false
	}

	translation_count := len(engine.history)
	replaced_count := match.replaced_count
	auto_split_prefix := false

	if replaced_count == 0 && translation_count > 0 && (formatted.attach_prev || formatted.glue && engine.history[translation_count - 1].glue) {
		if formatted.stitch && engine.history[translation_count - 1].glue {
			if !formatted_prepend_string(&formatted, formatted_stitch_delimiter(&formatted)) {
				return false
			}
		}
		replaced_count = 1
		formatted.attach_prev = true
		auto_split_prefix = true
	}

	if replaced_count > translation_count {
		return false
	}

	replace_start := translation_count - replaced_count
	old_text := ""
	old_text_alloc := ""
	old_text_owned := false
	if replaced_count > 0 {
		old_text_ok: bool
		old_text_alloc, old_text_ok = simple_engine_range_text(engine, replace_start, replaced_count)
		if !old_text_ok {
			return false
		}
		old_text_owned = true
		old_text = old_text_alloc
	}

	previous := simple_engine_previous_visible(engine, replace_start)
	next_text: string
	next_text_ok: bool
	if simple_engine_match_has_partial_prefix(match) {
		next_text, next_text_ok = simple_engine_build_partial_replacement_text(engine, match.partial_prefix_text, previous, &formatted)
	} else {
		next_text, next_text_ok = simple_engine_build_text(engine, old_text, previous, &formatted)
	}
	if !next_text_ok {
		if old_text_owned {
			owned_string_delete(old_text_alloc)
		}
		return false
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = next_text,
		next_attach = formatted.attach_next,
		glue = formatted.glue,
		previous_case_mode = previous_case_mode,
		previous_next_case = previous_next_case,
		has_case_state = true,
	}

	strokes := make([dynamic]u64)
	defer delete(strokes)
	if simple_engine_match_has_partial_prefix(match) {
		if replace_start >= translation_count || match.partial_prefix_stroke_count > len(engine.history[replace_start].strokes) {
			applied_translation_destroy(&next)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
		append_strokes(&strokes, engine.history[replace_start].strokes[:match.partial_prefix_stroke_count])
	} else if auto_split_prefix {
		for i := replace_start; i < translation_count; i += 1 {
			append_strokes(&strokes, engine.history[i].strokes[:])
		}
	}
	append_strokes(&next.strokes, strokes[:])
	split_prefix_stroke_count := len(strokes)
	append_strokes(&next.strokes, match.strokes[:])
	if simple_engine_match_has_partial_prefix(match) {
		if !applied_translation_set_split_prefix(&next, match.partial_prefix_text, match.partial_prefix_stroke_count) {
			applied_translation_destroy(&next)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
	} else if auto_split_prefix && simple_engine_text_has_prefix(next.text, old_text) {
		if !applied_translation_set_split_prefix(&next, old_text, split_prefix_stroke_count) {
			applied_translation_destroy(&next)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
	}
	if old_text_owned {
		owned_string_delete(old_text_alloc)
	}
	if replaced_count > 0 {
		next.replaced = make([dynamic]Applied_Translation)
		for i := replace_start; i < translation_count; i += 1 {
			append(&next.replaced, engine.history[i])
		}
	}

	resize(&engine.history, replace_start)
	append(&engine.history, next)
	if !simple_engine_record_key_combos(engine, &formatted) {
		return false
	}
	if len(formatted.mode_command) > 0 && !simple_engine_execute_mode_command(engine, formatted.mode_command) {
		return false
	}
	if len(formatted.plover_command) > 0 && !simple_engine_execute_plover_command(engine, formatted.plover_command) {
		return false
	}
	return true
}

simple_engine_undo_last :: proc(engine: ^Simple_Engine) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := engine.history[translation_count - 1]
	if last.has_case_state {
		engine.case_mode = last.previous_case_mode
		engine.next_case = last.previous_next_case
	}
	resize(&engine.history, translation_count - 1)
	for replaced in last.replaced {
		append(&engine.history, replaced)
	}
	applied_translation_destroy_without_replaced(&last)
	return true
}

simple_engine_repeat_last :: proc(engine: ^Simple_Engine, command_bits: u64) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := &engine.history[translation_count - 1]
	next := Applied_Translation {
		strokes = make([dynamic]u64),
		next_attach = last.next_attach,
		glue = last.glue,
		previous_case_mode = engine.case_mode,
		previous_next_case = engine.next_case,
		has_case_state = true,
	}
	append(&next.strokes, command_bits)

	buffer := make([dynamic]byte)
	defer delete(buffer)
	if len(last.text) > 0 && !last.glue && !last.next_attach && len(engine.spacing) > 0 && !text_starts_with_prefix(last.text, engine.spacing) {
		formatted_append_string(&buffer, engine.spacing)
	}
	formatted_append_string(&buffer, last.text)

	text, text_ok := clone_bytes_to_string(buffer[:])
	if !text_ok {
		applied_translation_destroy(&next)
		return false
	}
	next.text = text
	append(&engine.history, next)
	return true
}

simple_engine_execute_command :: proc(engine: ^Simple_Engine, command: string, bits: u64) -> bool {
	switch command {
	case "=undo":
		return simple_engine_undo_last(engine)
	case "=repeat_last_translation":
		return simple_engine_repeat_last(engine, bits)
	case:
		return true
	}
}

simple_engine_text_without_leading_spacing :: proc(engine: ^Simple_Engine, text: string) -> string {
	if text_starts_with_prefix(text, engine.spacing) {
		return text[len(engine.spacing):]
	}
	return text
}

simple_engine_apply_retro_delete_space :: proc(engine: ^Simple_Engine, match: ^Translation_Match) -> bool {
	translation_count := len(engine.history)
	if translation_count < 2 {
		return true
	}
	if engine.history[translation_count - 1].retro_space_command {
		return true
	}

	replace_start := translation_count - 2
	first := &engine.history[replace_start]
	second := &engine.history[replace_start + 1]
	buffer := make([dynamic]byte)
	defer delete(buffer)
	formatted_append_string(&buffer, first.text)
	formatted_append_string(&buffer, simple_engine_text_without_leading_spacing(engine, second.text))
	new_text, new_text_ok := clone_bytes_to_string(buffer[:])
	if !new_text_ok {
		return false
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = new_text,
		retro_space_command = true,
	}
	append_strokes(&next.strokes, match.strokes[:])
	next.replaced = make([dynamic]Applied_Translation)
	append(&next.replaced, engine.history[replace_start])
	append(&next.replaced, engine.history[replace_start + 1])
	resize(&engine.history, replace_start)
	append(&engine.history, next)
	return true
}

simple_engine_apply_retro_insert_space :: proc(engine: ^Simple_Engine, match: ^Translation_Match) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := &engine.history[translation_count - 1]
	if !last.retro_space_command || len(last.replaced) == 0 {
		return true
	}

	new_text, new_text_ok := applied_translation_replaced_text(last)
	if !new_text_ok {
		return false
	}
	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = new_text,
	}
	append_strokes(&next.strokes, match.strokes[:])
	next.replaced = make([dynamic]Applied_Translation)
	append(&next.replaced, engine.history[translation_count - 1])
	resize(&engine.history, translation_count - 1)
	append(&engine.history, next)
	return true
}

simple_engine_apply_retro_toggle_asterisk :: proc(engine: ^Simple_Engine) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := &engine.history[translation_count - 1]
	stroke_count := len(last.strokes)
	if stroke_count == 0 {
		return true
	}

	toggled_bits := last.strokes[stroke_count - 1] ~ steno_bit(.Star)
	return simple_engine_undo_last(engine) && simple_engine_translate_bits(engine, toggled_bits)
}

simple_engine_apply_retro_command :: proc(engine: ^Simple_Engine, match: ^Translation_Match, command: Retro_Command) -> bool {
	switch command {
	case .None:
		return true
	case .Toggle_Asterisk:
		return simple_engine_apply_retro_toggle_asterisk(engine)
	case .Delete_Space:
		return simple_engine_apply_retro_delete_space(engine, match)
	case .Insert_Space:
		return simple_engine_apply_retro_insert_space(engine, match)
	}
	return true
}

simple_engine_apply_retro_case :: proc(engine: ^Simple_Engine, match: ^Translation_Match, mode: Case_Mode) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := engine.history[translation_count - 1]
	cased_context, cased_ok := simple_engine_apply_case_to_text(last.text, mode)
	if !cased_ok {
		return false
	}
	cased, clone_ok := clone_string_ok(cased_context)
	delete(cased_context)
	if !clone_ok {
		return false
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = cased,
		previous_case_mode = engine.case_mode,
		previous_next_case = engine.next_case,
		has_case_state = true,
	}
	append_strokes(&next.strokes, match.strokes[:])
	next.replaced = make([dynamic]Applied_Translation)
	append(&next.replaced, last)
	resize(&engine.history, translation_count - 1)
	append(&engine.history, next)
	return true
}

simple_engine_ascii_equal_ignore_case :: proc(a: string, b: string) -> bool {
	if len(a) != len(b) {
		return false
	}
	for i in 0..<len(a) {
		if simple_engine_to_lower(a[i]) != simple_engine_to_lower(b[i]) {
			return false
		}
	}
	return true
}

simple_engine_execute_mode_command :: proc(engine: ^Simple_Engine, command: string) -> bool {
	separator := len(command)
	for i in 0..<len(command) {
		if command[i] == ':' {
			separator = i
			break
		}
	}
	name := command[:separator]
	argument := ""
	has_argument := separator < len(command)
	if has_argument {
		argument = command[separator + 1:]
	}

	if simple_engine_ascii_equal_ignore_case(name, "set_space") {
		return simple_engine_set_spacing(engine, argument)
	}
	if has_argument {
		return true
	}
	if simple_engine_ascii_equal_ignore_case(name, "caps") {
		engine.case_mode = .Upper
		return true
	}
	if simple_engine_ascii_equal_ignore_case(name, "title") {
		engine.case_mode = .Title
		return true
	}
	if simple_engine_ascii_equal_ignore_case(name, "lower") {
		engine.case_mode = .Lower
		return true
	}
	if simple_engine_ascii_equal_ignore_case(name, "snake") {
		return simple_engine_set_spacing(engine, "_")
	}
	if simple_engine_ascii_equal_ignore_case(name, "camel") {
		engine.case_mode = .Title
		engine.next_case = .Lower_First_Char
		return simple_engine_set_spacing(engine, "")
	}
	if simple_engine_ascii_equal_ignore_case(name, "reset") {
		engine.case_mode = .Normal
		engine.next_case = .Normal
		return simple_engine_set_spacing(engine, " ")
	}
	if simple_engine_ascii_equal_ignore_case(name, "reset_space") {
		return simple_engine_set_spacing(engine, " ")
	}
	if simple_engine_ascii_equal_ignore_case(name, "reset_case") {
		engine.case_mode = .Normal
		engine.next_case = .Normal
		return true
	}
	return true
}

simple_engine_record_key_combos :: proc(engine: ^Simple_Engine, formatted: ^Formatted_Text) -> bool {
	for combo in formatted.key_combos {
		copy, ok := clone_string_ok(combo)
		if !ok {
			return false
		}
		append(&engine.key_combos, copy)
	}
	return true
}

formatted_has_deferred_action :: proc(formatted: ^Formatted_Text) -> bool {
	return formatted.retro_command != .None ||
		formatted.stitch_last_word ||
		len(formatted.mode_command) > 0 ||
		len(formatted.plover_command) > 0 ||
		len(formatted.key_combos) > 0
}

simple_engine_execute_plover_command :: proc(engine: ^Simple_Engine, command: string) -> bool {
	if len(command) == 0 {
		return true
	}

	prefix := "toggle_dict:"
	if len(command) >= len(prefix) && formatted_ascii_equal_ignore_case(command[:len(prefix)], prefix) {
		if engine.dictionary_stack == nil {
			return true
		}
		return dictionary_stack_toggle(engine.dictionary_stack, command[len(prefix):])
	}
	return true
}

simple_engine_apply_stitch_last_word :: proc(engine: ^Simple_Engine, match: ^Translation_Match, formatted: ^Formatted_Text) -> bool {
	translation_count := len(engine.history)
	if match.replaced_count > translation_count {
		return false
	}

	replace_start := translation_count - match.replaced_count
	source_text := ""
	source_owned := false
	for {
		count := translation_count - replace_start
		source, source_ok := simple_engine_range_source_text(engine, replace_start, count)
		if !source_ok {
			if source_owned {
				owned_string_delete(source_text)
			}
			return false
		}
		if source_owned {
			owned_string_delete(source_text)
		}
		source_text = source
		source_owned = true
		if stitch_token_count(source_text) >= formatted.stitch_count || replace_start == 0 {
			break
		}
		replace_start -= 1
	}
	defer if source_owned {
		owned_string_delete(source_text)
	}

	actual_replaced_count := translation_count - replace_start
	new_text, new_text_ok := stitch_text_suffix(source_text, formatted.stitch_count, formatted_stitch_delimiter(formatted))
	if !new_text_ok {
		return false
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = new_text,
	}
	append_strokes(&next.strokes, match.strokes[:])
	if actual_replaced_count > 0 {
		next.replaced = make([dynamic]Applied_Translation)
		for i := replace_start; i < translation_count; i += 1 {
			append(&next.replaced, engine.history[i])
		}
	}

	resize(&engine.history, replace_start)
	append(&engine.history, next)
	return true
}

simple_engine_apply_suffix_match :: proc(engine: ^Simple_Engine, match: ^Translation_Match) -> bool {
	if len(match.suffix_base_translation) == 0 || len(match.suffix_translation) == 0 {
		return false
	}

	base, base_ok := format_translation_text_basic(match.suffix_base_translation)
	if !base_ok {
		return false
	}
	defer formatted_text_destroy(&base)

	suffix, suffix_ok := format_translation_text_basic(match.suffix_translation)
	if !suffix_ok {
		return false
	}
	defer formatted_text_destroy(&suffix)
	if formatted_has_deferred_action(&base) || formatted_has_deferred_action(&suffix) {
		return false
	}

	translation_count := len(engine.history)
	replaced_count := match.replaced_count
	if replaced_count > translation_count {
		return false
	}
	auto_split_prefix := false
	if replaced_count == 0 && translation_count > 0 && (base.attach_prev || base.glue && engine.history[translation_count - 1].glue) {
		replaced_count = 1
		base.attach_prev = true
		auto_split_prefix = true
	}

	previous_case_mode := engine.case_mode
	previous_next_case := engine.next_case
	if !simple_engine_apply_case_state_to_formatted(engine, &base) {
		return false
	}

	replace_start := translation_count - replaced_count
	old_text := ""
	old_text_alloc := ""
	old_text_owned := false
	if replaced_count > 0 {
		old_text_ok: bool
		old_text_alloc, old_text_ok = simple_engine_range_text(engine, replace_start, replaced_count)
		if !old_text_ok {
			return false
		}
		old_text_owned = true
		old_text = old_text_alloc
	}

	previous := simple_engine_previous_visible(engine, replace_start)
	base_text: string
	base_text_ok: bool
	if simple_engine_match_has_partial_prefix(match) {
		base_text, base_text_ok = simple_engine_build_partial_replacement_text(engine, match.partial_prefix_text, previous, &base)
	} else {
		base_text, base_text_ok = simple_engine_build_text(engine, old_text, previous, &base)
	}
	if !base_text_ok {
		if old_text_owned {
			owned_string_delete(old_text_alloc)
		}
		return false
	}

	final_text, final_text_ok := simple_engine_build_text(engine, base_text, nil, &suffix)
	owned_string_delete(base_text)
	if !final_text_ok {
		if old_text_owned {
			owned_string_delete(old_text_alloc)
		}
		return false
	}

	strokes := make([dynamic]u64)
	defer delete(strokes)
	if simple_engine_match_has_partial_prefix(match) {
		if replace_start >= translation_count || match.partial_prefix_stroke_count > len(engine.history[replace_start].strokes) {
			owned_string_delete(final_text)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
		append_strokes(&strokes, engine.history[replace_start].strokes[:match.partial_prefix_stroke_count])
	} else if auto_split_prefix {
		for i := replace_start; i < translation_count; i += 1 {
			append_strokes(&strokes, engine.history[i].strokes[:])
		}
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = final_text,
		next_attach = suffix.attach_next,
		glue = suffix.glue,
		previous_case_mode = previous_case_mode,
		previous_next_case = previous_next_case,
		has_case_state = true,
	}
	append_strokes(&next.strokes, strokes[:])
	split_prefix_stroke_count := len(strokes)
	append_strokes(&next.strokes, match.strokes[:])
	if simple_engine_match_has_partial_prefix(match) {
		if !applied_translation_set_split_prefix(&next, match.partial_prefix_text, match.partial_prefix_stroke_count) {
			applied_translation_destroy(&next)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
	} else if auto_split_prefix && simple_engine_text_has_prefix(next.text, old_text) {
		if !applied_translation_set_split_prefix(&next, old_text, split_prefix_stroke_count) {
			applied_translation_destroy(&next)
			if old_text_owned {
				owned_string_delete(old_text_alloc)
			}
			return false
		}
	}
	if old_text_owned {
		owned_string_delete(old_text_alloc)
	}
	if replaced_count > 0 {
		next.replaced = make([dynamic]Applied_Translation)
		for i := replace_start; i < translation_count; i += 1 {
			append(&next.replaced, engine.history[i])
		}
	}

	resize(&engine.history, replace_start)
	append(&engine.history, next)
	return true
}

simple_engine_translate_bits :: proc(engine: ^Simple_Engine, bits: u64) -> bool {
	if translation, found := dictionary_lookup_bits(engine.dictionary, bits); found && len(translation) > 0 && translation[0] == '=' {
		return simple_engine_execute_command(engine, translation, bits)
	}

	match, match_ok := simple_engine_find_match(engine, bits)
	if !match_ok {
		return false
	}
	defer translation_match_destroy(&match)

	return simple_engine_apply_match(engine, &match)
}

simple_engine_apply_single_stroke_translation :: proc(engine: ^Simple_Engine, bits: u64, translation: string) -> bool {
	strokes := [?]u64{bits}
	outline, outline_ok := stroke_sequence_to_string_alloc(strokes[:])
	if !outline_ok {
		return false
	}
	match := Translation_Match {
		translation = translation,
		strokes = make([dynamic]u64),
		replaced_count = 0,
		outline = outline,
		found = true,
	}
	append(&match.strokes, bits)
	defer translation_match_destroy(&match)
	return simple_engine_apply_match(engine, &match)
}

phrase_namespace_should_fallback_to_dictionary :: proc(bits: u64) -> bool {
	star_bits := steno_bit(.Star)
	allowed_bits := star_bits | steno_bit(.Num)
	return (bits & star_bits) != 0 && (bits & ~allowed_bits) == 0
}

simple_engine_translate_phrase_bits :: proc(engine: ^Simple_Engine, phrasing: ^Phrasing, bits: u64, mode: Phrase_Lookup_Mode) -> bool {
	if bits == 0 {
		return true
	}

	text, result := phrasing_lookup_mode(phrasing, bits, mode)
	defer owned_string_delete(text)
	switch result {
	case .Hit:
		return simple_engine_apply_single_stroke_translation(engine, bits, text)
	case .Error:
		return false
	case .Miss:
	}

	if phrase_namespace_should_fallback_to_dictionary(bits) {
		return simple_engine_translate_bits(engine, bits)
	}

	strokes := [?]u64{bits}
	raw, raw_ok := stroke_sequence_to_string_alloc(strokes[:])
	if !raw_ok {
		return false
	}
	defer owned_string_delete(raw)
	return simple_engine_apply_single_stroke_translation(engine, bits, raw)
}

simple_engine_render :: proc(engine: ^Simple_Engine) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	for translation in engine.history {
		formatted_append_string(&buffer, translation.text)
	}
	return clone_bytes_to_string(buffer[:])
}

translate_outline_sequence :: proc(dictionary: ^Dictionary, outlines: []string) -> (text: string, ok: bool) {
	engine: Simple_Engine
	simple_engine_init(&engine, dictionary)

	for outline in outlines {
		bits, parsed := stroke_string_to_bits(outline)
		if !parsed {
			simple_engine_destroy(&engine)
			return "", false
		}
		if !simple_engine_translate_bits(&engine, bits) {
			simple_engine_destroy(&engine)
			return "", false
		}
	}

	text, ok = simple_engine_render(&engine)
	simple_engine_destroy(&engine)
	return text, ok
}
