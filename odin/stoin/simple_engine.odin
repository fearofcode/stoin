package stoin

import "base:runtime"

TRANSLATION_MATCH_MAX_STROKES :: 100

Applied_Translation :: struct {
	strokes:      [dynamic]u64,
	text:         string,
	next_attach:  bool,
	glue:         bool,
	replaced:     [dynamic]Applied_Translation,
	retro_space_command: bool,
}

Translation_Match :: struct {
	translation:    string,
	suffix_base_translation: string,
	suffix_translation: string,
	suffix_match:    bool,
	strokes:         [dynamic]u64,
	replaced_count:  int,
	outline:         string,
	found:           bool,
}

Simple_Engine :: struct {
	dictionary: ^Dictionary,
	history:    [dynamic]Applied_Translation,
}

simple_engine_init :: proc(engine: ^Simple_Engine, dictionary: ^Dictionary) {
	engine^ = {}
	engine.dictionary = dictionary
	engine.history = make([dynamic]Applied_Translation)
}

simple_engine_destroy :: proc(engine: ^Simple_Engine) {
	for i in 0..<len(engine.history) {
		applied_translation_destroy(&engine.history[i])
	}
	delete(engine.history)
	engine^ = {}
}

applied_translation_destroy :: proc(translation: ^Applied_Translation) {
	delete(translation.strokes)
	owned_string_delete(translation.text)
	for i in 0..<len(translation.replaced) {
		applied_translation_destroy(&translation.replaced[i])
	}
	delete(translation.replaced)
	translation^ = {}
}

applied_translation_destroy_without_replaced :: proc(translation: ^Applied_Translation) {
	delete(translation.strokes)
	owned_string_delete(translation.text)
	delete(translation.replaced)
	translation^ = {}
}

translation_match_destroy :: proc(match: ^Translation_Match) {
	delete(match.strokes)
	owned_string_delete(match.outline)
	match^ = {}
}

clone_bytes_to_string :: proc(data: []byte) -> (string, bool) {
	cloned := make([]byte, len(data), runtime.heap_allocator())
	if len(data) > 0 && len(cloned) != len(data) {
		return "", false
	}
	copy(cloned, data)
	return string(cloned), true
}

clone_string_ok :: proc(s: string) -> (string, bool) {
	return clone_bytes_to_string(transmute([]byte)s)
}

owned_string_delete :: proc(text: string) {
	if len(text) != 0 {
		delete(text, runtime.heap_allocator())
	}
}

stroke_sequence_to_string_alloc :: proc(strokes: []u64) -> (outline: string, ok: bool) {
	buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
	n, formatted := strokes_to_outline_key(strokes, buffer[:])
	if !formatted {
		return "", false
	}
	return clone_bytes_to_string(buffer[:n])
}

simple_engine_lookup_stroke_limit :: proc(dictionary: ^Dictionary) -> int {
	max_strokes := dictionary_longest_key(dictionary)
	if max_strokes == 0 || max_strokes > TRANSLATION_MATCH_MAX_STROKES {
		max_strokes = TRANSLATION_MATCH_MAX_STROKES
	}
	return max_strokes
}

append_strokes :: proc(out: ^[dynamic]u64, strokes: []u64) {
	for stroke in strokes {
		append(out, stroke)
	}
}

bits_after_steno_key :: proc(key: Steno_Key) -> u64 {
	bits: u64 = 0
	for candidate_index := int(key) + 1; candidate_index <= int(Steno_Key.Right_Z); candidate_index += 1 {
		bits |= steno_bit(Steno_Key(candidate_index))
	}
	return bits
}

simple_engine_try_suffix_match :: proc(dictionary: ^Dictionary, candidate: []u64, replaced_count: int, match: ^Translation_Match, best_candidate: ^[dynamic]u64) -> (found: bool, ok: bool) {
	if len(candidate) == 0 {
		return false, true
	}

	suffix_keys := [?]Steno_Key{.Right_Z, .Right_D, .Right_S, .Right_G}
	last_stroke := candidate[len(candidate) - 1]
	for suffix_key in suffix_keys {
		suffix_bit := steno_bit(suffix_key)
		if (last_stroke & suffix_bit) == 0 || (last_stroke & bits_after_steno_key(suffix_key)) != 0 {
			continue
		}

		base_candidate := make([dynamic]u64)
		append_strokes(&base_candidate, candidate)
		base_candidate[len(base_candidate) - 1] &= ~suffix_bit
		if base_candidate[len(base_candidate) - 1] == 0 {
			delete(base_candidate)
			continue
		}

		base_translation, base_found := dictionary_lookup_strokes(dictionary, base_candidate[:])
		if !base_found || len(base_translation) > 0 && base_translation[0] == '=' {
			delete(base_candidate)
			continue
		}
		suffix_translation, suffix_found := dictionary_lookup_bits(dictionary, suffix_bit)
		if !suffix_found || len(suffix_translation) > 0 && suffix_translation[0] == '=' {
			delete(base_candidate)
			continue
		}
		delete(base_candidate)

		match.translation = ""
		match.suffix_base_translation = base_translation
		match.suffix_translation = suffix_translation
		match.suffix_match = true
		match.replaced_count = replaced_count
		match.found = true
		resize(best_candidate, 0)
		append_strokes(best_candidate, candidate)
		return true, true
	}

	return false, true
}

simple_engine_find_match :: proc(engine: ^Simple_Engine, bits: u64) -> (match: Translation_Match, ok: bool) {
	max_strokes := simple_engine_lookup_stroke_limit(engine.dictionary)
	candidate := make([dynamic]u64)
	defer delete(candidate)
	append(&candidate, bits)
	best_candidate := make([dynamic]u64)
	defer delete(best_candidate)

	replaced_count := 0
	if translation, found := dictionary_lookup_strokes(engine.dictionary, candidate[:]); found {
		if len(translation) == 0 || translation[0] != '=' {
			match.translation = translation
			match.suffix_base_translation = ""
			match.suffix_translation = ""
			match.suffix_match = false
			match.replaced_count = replaced_count
			match.found = true
			append_strokes(&best_candidate, candidate[:])
		}
	} else {
		if _, suffix_ok := simple_engine_try_suffix_match(engine.dictionary, candidate[:], replaced_count, &match, &best_candidate); !suffix_ok {
			return {}, false
		}
	}

	for i := len(engine.history); i > 0 && len(candidate) < max_strokes; {
		i -= 1
		previous := &engine.history[i]
		if len(previous.strokes) == 0 || len(candidate) + len(previous.strokes) > max_strokes {
			break
		}

		next_candidate := make([dynamic]u64)
		append_strokes(&next_candidate, previous.strokes[:])
		append_strokes(&next_candidate, candidate[:])
		delete(candidate)
		candidate = next_candidate
		replaced_count += 1

		if translation, found := dictionary_lookup_strokes(engine.dictionary, candidate[:]); found {
			if len(translation) == 0 || translation[0] != '=' {
				match.translation = translation
				match.suffix_base_translation = ""
				match.suffix_translation = ""
				match.suffix_match = false
				match.replaced_count = replaced_count
				match.found = true
				resize(&best_candidate, 0)
				append_strokes(&best_candidate, candidate[:])
			}
		} else {
			if _, suffix_ok := simple_engine_try_suffix_match(engine.dictionary, candidate[:], replaced_count, &match, &best_candidate); !suffix_ok {
				translation_match_destroy(&match)
				return {}, false
			}
		}
	}

	if !match.found {
		current := [?]u64{bits}
		outline, outline_ok := stroke_sequence_to_string_alloc(current[:])
		if !outline_ok {
			return {}, false
		}
		match.translation = outline
		match.outline = outline
		match.found = false
		match.strokes = make([dynamic]u64)
		append_strokes(&match.strokes, current[:])
	} else {
		outline, outline_ok := stroke_sequence_to_string_alloc(best_candidate[:])
		if !outline_ok {
			translation_match_destroy(&match)
			return {}, false
		}
		match.outline = outline
		match.strokes = make([dynamic]u64)
		append_strokes(&match.strokes, best_candidate[:])
	}

	return match, true
}

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

text_starts_with_spacing :: proc(text: string) -> bool {
	return len(text) > 0 && text[0] == ' '
}

text_ends_with_spacing :: proc(text: string) -> bool {
	return len(text) > 0 && text[len(text) - 1] == ' '
}

simple_engine_should_prepend_spacing :: proc(previous: ^Applied_Translation, formatted: ^Formatted_Text) -> bool {
	if len(formatted.text) == 0 || formatted.attach_prev || formatted.glue || text_starts_with_spacing(formatted.text) {
		return false
	}
	if previous == nil {
		return false
	}
	if previous.next_attach || text_ends_with_spacing(previous.text) {
		return false
	}
	return true
}

simple_engine_build_text :: proc(old_text: string, previous: ^Applied_Translation, formatted: ^Formatted_Text) -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	if formatted.attach_prev {
		if len(formatted.ortho_suffix) > 0 {
			word_start, word_end := last_word_bounds(old_text)
			formatted_append_string(&buffer, old_text[:word_start])
			joined, joined_ok := orthography_apply_basic(old_text[word_start:word_end], formatted.ortho_suffix)
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
	} else if simple_engine_should_prepend_spacing(previous, formatted) {
		append(&buffer, ' ')
	}
	if len(formatted.ortho_suffix) == 0 {
		formatted_append_string(&buffer, formatted.text)
	}
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

	translation_count := len(engine.history)
	replaced_count := match.replaced_count
	strokes := make([dynamic]u64)
	defer delete(strokes)

	if replaced_count == 0 && translation_count > 0 && (formatted.attach_prev || formatted.glue && engine.history[translation_count - 1].glue) {
		if formatted.stitch && engine.history[translation_count - 1].glue {
			if !formatted_prepend_string(&formatted, formatted_stitch_delimiter(&formatted)) {
				return false
			}
		}
		replaced_count = 1
		formatted.attach_prev = true
		append_strokes(&strokes, engine.history[translation_count - 1].strokes[:])
	}
	append_strokes(&strokes, match.strokes[:])

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
	next_text, next_text_ok := simple_engine_build_text(old_text, previous, &formatted)
	if old_text_owned {
		owned_string_delete(old_text_alloc)
	}
	if !next_text_ok {
		return false
	}

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = next_text,
		next_attach = formatted.attach_next,
		glue = formatted.glue,
	}
	append_strokes(&next.strokes, strokes[:])
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

simple_engine_undo_last :: proc(engine: ^Simple_Engine) -> bool {
	translation_count := len(engine.history)
	if translation_count == 0 {
		return true
	}

	last := engine.history[translation_count - 1]
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
	}
	append(&next.strokes, command_bits)

	buffer := make([dynamic]byte)
	defer delete(buffer)
	if len(last.text) > 0 && !last.glue && !last.next_attach && !text_starts_with_spacing(last.text) {
		append(&buffer, ' ')
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

simple_engine_text_without_leading_spacing :: proc(text: string) -> string {
	if len(text) > 0 && text[0] == ' ' {
		return text[1:]
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
	formatted_append_string(&buffer, simple_engine_text_without_leading_spacing(second.text))
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

	translation_count := len(engine.history)
	replaced_count := match.replaced_count
	if replaced_count > translation_count {
		return false
	}
	if replaced_count == 0 && translation_count > 0 && (base.attach_prev || base.glue && engine.history[translation_count - 1].glue) {
		replaced_count = 1
		base.attach_prev = true
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
	base_text, base_text_ok := simple_engine_build_text(old_text, previous, &base)
	if old_text_owned {
		owned_string_delete(old_text_alloc)
	}
	if !base_text_ok {
		return false
	}

	final_text, final_text_ok := simple_engine_build_text(base_text, nil, &suffix)
	owned_string_delete(base_text)
	if !final_text_ok {
		return false
	}

	strokes := make([dynamic]u64)
	defer delete(strokes)
	if replaced_count != match.replaced_count {
		for i := replace_start; i < translation_count; i += 1 {
			append_strokes(&strokes, engine.history[i].strokes[:])
		}
	}
	append_strokes(&strokes, match.strokes[:])

	next := Applied_Translation {
		strokes = make([dynamic]u64),
		text = final_text,
		next_attach = suffix.attach_next,
		glue = suffix.glue,
	}
	append_strokes(&next.strokes, strokes[:])
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
