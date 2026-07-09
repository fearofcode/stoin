package stoin

simple_engine_lookup_stroke_limit :: proc(dictionary: ^Dictionary) -> int {
	max_strokes := dictionary_longest_key(dictionary)
	if max_strokes == 0 || max_strokes > TRANSLATION_MATCH_MAX_STROKES {
		max_strokes = TRANSLATION_MATCH_MAX_STROKES
	}
	return max_strokes
}

simple_engine_history_stroke_count :: proc(engine: ^Simple_Engine) -> int {
	if engine == nil {
		return 0
	}
	stroke_count := 0
	for translation in engine.history {
		stroke_count += len(translation.strokes)
	}
	return stroke_count
}

simple_engine_compact_history :: proc(engine: ^Simple_Engine, keep_strokes: int) {
	if engine == nil || keep_strokes <= 0 || len(engine.history) == 0 {
		return
	}

	retained_strokes := 0
	retained_translations := 0
	for i := len(engine.history); i > 0; {
		i -= 1
		retained_translations += 1
		retained_strokes += len(engine.history[i].strokes)
		if retained_strokes >= keep_strokes {
			break
		}
	}

	dropped_translations := len(engine.history) - retained_translations
	if dropped_translations <= 0 {
		return
	}

	for i in 0..<dropped_translations {
		applied_translation_destroy(&engine.history[i])
	}
	for i in 0..<retained_translations {
		engine.history[i] = engine.history[dropped_translations + i]
	}
	resize(&engine.history, retained_translations)
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
	if _, found := dictionary_lookup_bits(dictionary, last_stroke); found {
		return false, true
	}
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

simple_engine_try_candidate_match :: proc(
	dictionary: ^Dictionary,
	candidate: []u64,
	replaced_count: int,
	match: ^Translation_Match,
	best_candidate: ^[dynamic]u64,
	partial_prefix_text: string = "",
	partial_prefix_stroke_count: int = 0,
) -> (found: bool, ok: bool) {
	if translation, dictionary_found := dictionary_lookup_strokes(dictionary, candidate); dictionary_found {
		if len(translation) == 0 || translation[0] != '=' {
			match.translation = translation
			match.suffix_base_translation = ""
			match.suffix_translation = ""
			match.suffix_match = false
			match.replaced_count = replaced_count
			match.found = true
			if partial_prefix_stroke_count > 0 {
				if !translation_match_set_partial_prefix(match, partial_prefix_text, partial_prefix_stroke_count) {
					return false, false
				}
			} else {
				translation_match_clear_partial_prefix(match)
			}
			resize(best_candidate, 0)
			append_strokes(best_candidate, candidate)
			return true, true
		}
		return false, true
	}

	suffix_found, suffix_ok := simple_engine_try_suffix_match(dictionary, candidate, replaced_count, match, best_candidate)
	if !suffix_ok {
		return false, false
	}
	if suffix_found {
		if partial_prefix_stroke_count > 0 {
			if !translation_match_set_partial_prefix(match, partial_prefix_text, partial_prefix_stroke_count) {
				return false, false
			}
		} else {
			translation_match_clear_partial_prefix(match)
		}
	}
	return suffix_found, true
}

simple_engine_text_has_prefix :: proc(text: string, prefix: string) -> bool {
	return len(prefix) <= len(text) && text[:len(prefix)] == prefix
}

simple_engine_try_partial_candidate_match :: proc(
	engine: ^Simple_Engine,
	previous: ^Applied_Translation,
	candidate: []u64,
	replaced_count: int,
	match: ^Translation_Match,
	best_candidate: ^[dynamic]u64,
	max_strokes: int,
) -> (ok: bool) {
	prefix_stroke_count := previous.split_prefix_stroke_count
	if prefix_stroke_count <= 0 ||
	   prefix_stroke_count >= len(previous.strokes) ||
	   !simple_engine_text_has_prefix(previous.text, previous.split_prefix_text) {
		return true
	}

	suffix := previous.strokes[prefix_stroke_count:]
	if len(suffix) == 0 || len(suffix) + len(candidate) > max_strokes {
		return true
	}

	partial_candidate := make([dynamic]u64)
	defer delete(partial_candidate)
	append_strokes(&partial_candidate, suffix)
	append_strokes(&partial_candidate, candidate)

	_, candidate_ok := simple_engine_try_candidate_match(
		engine.dictionary,
		partial_candidate[:],
		replaced_count + 1,
		match,
		best_candidate,
		previous.split_prefix_text,
		prefix_stroke_count,
	)
	return candidate_ok
}

simple_engine_find_match :: proc(engine: ^Simple_Engine, bits: u64) -> (match: Translation_Match, ok: bool) {
	max_strokes := simple_engine_lookup_stroke_limit(engine.dictionary)
	candidate := make([dynamic]u64)
	defer delete(candidate)
	append(&candidate, bits)
	best_candidate := make([dynamic]u64)
	defer delete(best_candidate)

	replaced_count := 0
	if _, candidate_ok := simple_engine_try_candidate_match(engine.dictionary, candidate[:], replaced_count, &match, &best_candidate); !candidate_ok {
		return {}, false
	}

	for i := len(engine.history); i > 0 && len(candidate) < max_strokes; {
		i -= 1
		previous := &engine.history[i]
		if len(previous.strokes) == 0 {
			break
		}

		if !simple_engine_try_partial_candidate_match(engine, previous, candidate[:], replaced_count, &match, &best_candidate, max_strokes) {
			translation_match_destroy(&match)
			return {}, false
		}

		if len(candidate) + len(previous.strokes) > max_strokes {
			break
		}

		next_candidate := make([dynamic]u64)
		append_strokes(&next_candidate, previous.strokes[:])
		append_strokes(&next_candidate, candidate[:])
		delete(candidate)
		candidate = next_candidate
		replaced_count += 1

		if _, candidate_ok := simple_engine_try_candidate_match(engine.dictionary, candidate[:], replaced_count, &match, &best_candidate); !candidate_ok {
			translation_match_destroy(&match)
			return {}, false
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

