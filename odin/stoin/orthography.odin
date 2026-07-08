package stoin

import "core:os"

Orthography :: struct {
	words: map[string]bool,
}

orthography_init :: proc(orthography: ^Orthography) {
	orthography^ = {}
	orthography.words = make(map[string]bool)
}

orthography_destroy :: proc(orthography: ^Orthography) {
	if orthography.words != nil {
		for word, _ in orthography.words {
			owned_string_delete(word)
		}
		delete(orthography.words)
	}
	orthography^ = {}
}

orthography_is_space :: proc(c: byte) -> bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
}

orthography_load :: proc(orthography: ^Orthography, path: string) -> bool {
	if orthography.words == nil {
		orthography.words = make(map[string]bool)
	}

	data, read_err := os.read_entire_file(path, context.allocator)
	if read_err != nil {
		return false
	}
	defer delete(data)

	for i := 0; i < len(data); {
		for i < len(data) && (data[i] == '\n' || data[i] == '\r') {
			i += 1
		}
		start := i
		for i < len(data) && !orthography_is_space(data[i]) {
			i += 1
		}
		if i > start {
			word := string(data[start:i])
			if _, found := orthography.words[word]; !found {
				copy, ok := clone_string_ok(word)
				if !ok {
					return false
				}
				orthography.words[copy] = true
			}
		}
		for i < len(data) && data[i] != '\n' {
			i += 1
		}
	}
	return true
}

orthography_word_count :: proc(orthography: ^Orthography) -> int {
	if orthography == nil || orthography.words == nil {
		return 0
	}
	return len(orthography.words)
}

orthography_contains_word :: proc(orthography: ^Orthography, word: string) -> bool {
	if orthography == nil || orthography.words == nil {
		return false
	}
	_, found := orthography.words[word]
	return found
}

orthography_starts_with :: proc(s: string, prefix: string) -> bool {
	return len(s) >= len(prefix) && s[:len(prefix)] == prefix
}

orthography_ends_with :: proc(s: string, suffix: string) -> bool {
	return len(s) >= len(suffix) && s[len(s) - len(suffix):] == suffix
}

orthography_char_in :: proc(c: byte, set: string) -> bool {
	for item in set {
		if byte(item) == c {
			return true
		}
	}
	return false
}

orthography_is_consonant :: proc(c: byte) -> bool {
	return orthography_char_in(c, "bcdfghjklmnpqrstvwxz")
}

orthography_append_parts :: proc(a: string, b: string, c: string = "") -> (string, bool) {
	buffer := make([dynamic]byte)
	defer delete(buffer)

	formatted_append_string(&buffer, a)
	formatted_append_string(&buffer, b)
	formatted_append_string(&buffer, c)
	return clone_bytes_to_string(buffer[:])
}

orthography_rule_ic_ally :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "c") || suffix != "ly" || len(word) < 3 {
		return "", false
	}
	c := word[len(word) - 2]
	if !orthography_char_in(c, "aeiou") {
		return "", false
	}
	return orthography_append_parts(word, "ally")
}

orthography_rule_le_ly :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "le") || suffix != "ly" || len(word) < 3 {
		return "", false
	}
	if !orthography_char_in(word[len(word) - 3], "aeioubmnp") {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 2], "ly")
}

orthography_rule_sibilant_plural :: proc(word: string, suffix: string) -> (string, bool) {
	if suffix != "s" {
		return "", false
	}
	if orthography_ends_with(word, "s") || orthography_ends_with(word, "sh") ||
	   orthography_ends_with(word, "x") || orthography_ends_with(word, "z") ||
	   orthography_ends_with(word, "zh") {
		return orthography_append_parts(word, "es")
	}
	return "", false
}

orthography_rule_ch_plural :: proc(word: string, suffix: string) -> (string, bool) {
	if suffix != "s" || !orthography_ends_with(word, "ch") || len(word) < 3 {
		return "", false
	}

	before_ch := word[len(word) - 3]
	has_vowel_pair := false
	if len(word) >= 4 {
		two_before := word[len(word) - 4:len(word) - 2]
		has_vowel_pair = two_before == "oa" || two_before == "ea" || two_before == "ee" ||
			two_before == "oo" || two_before == "au" || two_before == "ou"
	}

	valid_single := before_ch == 'i' || before_ch == 'l' || before_ch == 'n' || before_ch == 't'
	valid_r := false
	if before_ch == 'r' {
		if len(word) < 5 {
			valid_r = true
		} else {
			before_r := word[len(word) - 5:len(word) - 3]
			valid_r = before_r != "ga" && before_r != "ia" && before_r != "na"
		}
	}

	if !has_vowel_pair && !valid_single && !valid_r {
		return "", false
	}
	return orthography_append_parts(word, "es")
}

orthography_rule_consonant_y_plural :: proc(word: string, suffix: string) -> (string, bool) {
	if suffix != "s" || len(word) < 3 || !orthography_ends_with(word, "y") {
		return "", false
	}
	if !orthography_is_consonant(word[len(word) - 2]) {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 1], "ies")
}

orthography_rule_ying :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "ie") || len(word) < 3 || suffix != "ing" {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 2], "ying")
}

orthography_rule_ie_ed :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "ie") || len(word) < 3 || suffix != "ed" {
		return "", false
	}
	return orthography_append_parts(word, "d")
}

orthography_rule_yist :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "y") || len(word) < 4 || suffix != "ist" {
		return "", false
	}
	if !orthography_char_in(word[len(word) - 2], "cdfghlmnpr") {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 1], "ist")
}

orthography_rule_y_to_i :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "y") || len(word) < 3 || len(suffix) == 0 {
		return "", false
	}
	if !orthography_is_consonant(word[len(word) - 2]) || suffix[0] == 'i' || suffix[0] == 'y' {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 1], "i", suffix)
}

orthography_rule_ish :: proc(word: string, suffix: string) -> (string, bool) {
	if (!orthography_ends_with(word, "ar") && !orthography_ends_with(word, "er") && !orthography_ends_with(word, "or")) ||
	   len(word) < 3 || !orthography_ends_with(suffix, "ish") {
		return "", false
	}
	return orthography_append_parts(word, suffix)
}

orthography_rule_silent_e_ing :: proc(word: string, suffix: string) -> (string, bool) {
	if !orthography_ends_with(word, "e") || len(word) < 3 || len(suffix) == 0 {
		return "", false
	}
	before_e := word[len(word) - 2]
	if (!orthography_is_consonant(before_e) && before_e != 'u') || !orthography_char_in(suffix[0], "aeiouy") {
		return "", false
	}
	return orthography_append_parts(word[:len(word) - 1], suffix)
}

orthography_rule_double_consonant_suffix :: proc(word: string, suffix: string) -> (string, bool) {
	if len(word) < 3 || len(suffix) == 0 {
		return "", false
	}

	final := word[len(word) - 1]
	if !orthography_char_in(final, "bcdfgklmnprtvz") || !orthography_char_in(word[len(word) - 2], "aeiou") {
		return "", false
	}
	antepenult := word[len(word) - 3]
	if !orthography_char_in(antepenult, "bcdfghjklmnprstvwxyz") {
		if len(word) < 4 || word[len(word) - 4:len(word) - 2] != "qu" {
			return "", false
		}
	}
	if !orthography_char_in(suffix[0], "aeiouy") {
		return "", false
	}

	doubled := [1]byte{final}
	return orthography_append_parts(word, string(doubled[:]), suffix)
}

orthography_suffix_alias :: proc(suffix: string) -> (string, bool) {
	switch suffix {
	case "able":
		return "ible", true
	case "ability":
		return "ibility", true
	}
	return "", false
}

orthography_split_suffix :: proc(suffix: string) -> (core_suffix: string, rest: string) {
	for i in 0..<len(suffix) {
		if suffix[i] == ' ' {
			return suffix[:i], suffix[i:]
		}
	}
	return suffix, ""
}

orthography_owned_with_rest :: proc(candidate: string, rest: string) -> (string, bool) {
	if len(rest) == 0 {
		return candidate, true
	}
	result, ok := orthography_append_parts(candidate, rest)
	owned_string_delete(candidate)
	return result, ok
}

orthography_apply :: proc(orthography: ^Orthography, word: string, suffix: string) -> (string, bool) {
	if len(word) == 0 {
		return clone_string_ok(suffix)
	}

	core_suffix, rest := orthography_split_suffix(suffix)
	rules := [?]proc(string, string) -> (string, bool) {
		orthography_rule_ic_ally,
		orthography_rule_le_ly,
		orthography_rule_sibilant_plural,
		orthography_rule_ch_plural,
		orthography_rule_consonant_y_plural,
		orthography_rule_ying,
		orthography_rule_ie_ed,
		orthography_rule_yist,
		orthography_rule_y_to_i,
		orthography_rule_ish,
		orthography_rule_silent_e_ing,
		orthography_rule_double_consonant_suffix,
	}

	if alias, has_alias := orthography_suffix_alias(core_suffix); has_alias {
		for rule in rules {
			if candidate, ok := rule(word, alias); ok {
				if orthography_contains_word(orthography, candidate) {
					return orthography_owned_with_rest(candidate, rest)
				}
				owned_string_delete(candidate)
			}
		}
	}

	first_non_dictionary_match := ""
	has_first_non_dictionary_match := false
	for rule in rules {
		if candidate, ok := rule(word, core_suffix); ok {
			if orthography_contains_word(orthography, candidate) {
				if has_first_non_dictionary_match {
					owned_string_delete(first_non_dictionary_match)
				}
				return orthography_owned_with_rest(candidate, rest)
			}
			if !has_first_non_dictionary_match {
				first_non_dictionary_match = candidate
				has_first_non_dictionary_match = true
			} else {
				owned_string_delete(candidate)
			}
		}
	}

	plain, plain_ok := orthography_append_parts(word, core_suffix)
	if !plain_ok {
		if has_first_non_dictionary_match {
			owned_string_delete(first_non_dictionary_match)
		}
		return "", false
	}
	if orthography_contains_word(orthography, plain) {
		if has_first_non_dictionary_match {
			owned_string_delete(first_non_dictionary_match)
		}
		return orthography_owned_with_rest(plain, rest)
	}

	if has_first_non_dictionary_match {
		owned_string_delete(plain)
		return orthography_owned_with_rest(first_non_dictionary_match, rest)
	}
	return orthography_owned_with_rest(plain, rest)
}

orthography_apply_basic :: proc(word: string, suffix: string) -> (string, bool) {
	return orthography_apply(nil, word, suffix)
}

last_word_bounds :: proc(text: string) -> (start: int, end: int) {
	end = len(text)
	for end > 0 && text[end - 1] == ' ' {
		end -= 1
	}
	start = end
	for start > 0 && text[start - 1] != ' ' {
		start -= 1
	}
	return start, end
}
