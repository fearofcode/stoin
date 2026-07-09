package stoin

import "core:encoding/json"
import "core:os"
import "core:strings"

DICTIONARY_MAX_OUTLINE_BYTES :: 4096

Dictionary :: struct {
	entries:     map[string]string,
	longest_key: int,
}

string_compare_ascii :: proc(a: string, b: string) -> int {
	length := len(a)
	if len(b) < length {
		length = len(b)
	}
	for i in 0..<length {
		if a[i] < b[i] {
			return -1
		}
		if a[i] > b[i] {
			return 1
		}
	}
	if len(a) < len(b) {
		return -1
	}
	if len(a) > len(b) {
		return 1
	}
	return 0
}

append_bytes_to_buffer :: proc(out: []byte, index: ^int, data: []byte) -> bool {
	if index^ + len(data) > len(out) {
		return false
	}
	copy(out[index^:index^ + len(data)], data)
	index^ += len(data)
	return true
}

outline_key_stroke_count :: proc(outline: string) -> int {
	if len(outline) == 0 {
		return 0
	}

	count := 1
	for i in 0..<len(outline) {
		if outline[i] == '/' {
			count += 1
		}
	}
	return count
}

strokes_to_outline_key :: proc(strokes: []u64, out: []byte) -> (n: int, ok: bool) {
	if len(strokes) == 0 || len(out) == 0 {
		return 0, false
	}

	index := 0
	for stroke, i in strokes {
		stroke_buffer: [64]byte
		stroke_len, stroke_ok := chord_bits_to_string(stroke, stroke_buffer[:])
		if !stroke_ok {
			return 0, false
		}
		if i != 0 && !append_byte_to_buffer(out, &index, '/') {
			return 0, false
		}
		if !append_bytes_to_buffer(out, &index, stroke_buffer[:stroke_len]) {
			return 0, false
		}
	}

	return index, true
}

outline_to_canonical_key :: proc(outline: string, out: []byte) -> (n: int, stroke_count: int, ok: bool) {
	if len(outline) == 0 || len(out) == 0 {
		return 0, 0, false
	}

	index := 0
	segment_start := 0
	for {
		segment_end := segment_start
		for segment_end < len(outline) && outline[segment_end] != '/' {
			segment_end += 1
		}

		length := segment_end - segment_start
		if length == 0 || length >= 64 {
			return 0, 0, false
		}

		segment := outline[segment_start:segment_end]
		bits, parsed := stroke_string_to_bits(segment)
		if !parsed {
			return 0, 0, false
		}

		stroke_buffer: [64]byte
		stroke_len, formatted := chord_bits_to_string(bits, stroke_buffer[:])
		if !formatted {
			return 0, 0, false
		}

		if stroke_count != 0 && !append_byte_to_buffer(out, &index, '/') {
			return 0, 0, false
		}
		if !append_bytes_to_buffer(out, &index, stroke_buffer[:stroke_len]) {
			return 0, 0, false
		}
		stroke_count += 1

		if segment_end == len(outline) {
			break
		}
		segment_start = segment_end + 1
	}

	if stroke_count == 0 {
		return 0, 0, false
	}
	return index, stroke_count, true
}

dictionary_init :: proc(dictionary: ^Dictionary) {
	dictionary^ = {}
	dictionary.entries = make(map[string]string)
}

dictionary_destroy :: proc(dictionary: ^Dictionary) {
	if dictionary.entries != nil {
		dictionary_destroy_string_map(&dictionary.entries)
	}
	dictionary^ = {}
}

dictionary_destroy_string_map :: proc(entries: ^map[string]string) {
	if entries^ == nil {
		return
	}

	for key, value in entries^ {
		delete(key)
		delete(value)
	}
	delete(entries^)
	entries^ = nil
}

dictionary_put :: proc(dictionary: ^Dictionary, canonical: string, stroke_count: int, translation: string) -> bool {
	if dictionary.entries == nil {
		dictionary.entries = make(map[string]string)
	}

	value_clone, value_err := strings.clone(translation)
	if value_err != nil {
		return false
	}

	if old_value, found := dictionary.entries[canonical]; found {
		delete(old_value)
		dictionary.entries[canonical] = value_clone
	} else {
		key_clone, key_err := strings.clone(canonical)
		if key_err != nil {
			delete(value_clone)
			return false
		}
		dictionary.entries[key_clone] = value_clone
	}

	if stroke_count > dictionary.longest_key {
		dictionary.longest_key = stroke_count
	}
	return true
}

dictionary_load :: proc(dictionary: ^Dictionary, path: string) -> bool {
	data, read_err := os.read_entire_file(path, context.allocator)
	if read_err != nil {
		return false
	}
	defer delete(data)

	raw_entries := make(map[string]string)
	defer dictionary_destroy_string_map(&raw_entries)

	unmarshal_err := json.unmarshal(data, &raw_entries)
	if unmarshal_err != nil {
		return false
	}

	for outline, translation in raw_entries {
		canonical_buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
		canonical_len, stroke_count, canonical_ok := outline_to_canonical_key(outline, canonical_buffer[:])
		if canonical_ok {
			canonical := string(canonical_buffer[:canonical_len])
			if !dictionary_put(dictionary, canonical, stroke_count, translation) {
				return false
			}
		}
	}

	return true
}

dictionary_load_many :: proc(dictionary: ^Dictionary, paths: []string) -> bool {
	if len(paths) == 0 {
		return false
	}
	for path in paths {
		if len(path) == 0 || !dictionary_load(dictionary, path) {
			return false
		}
	}
	return true
}

dictionary_dump_json :: proc(dictionary: ^Dictionary, path: string) -> bool {
	if dictionary == nil || len(path) == 0 || dictionary.entries == nil {
		return false
	}

	options := json.Marshal_Options {
		spec = .JSON,
		pretty = true,
		use_spaces = true,
		spaces = 4,
		sort_maps_by_key = true,
	}
	data, marshal_err := json.marshal(dictionary.entries, options)
	if marshal_err != nil {
		return false
	}
	defer delete(data)

	file, create_err := os.create(path)
	if create_err != nil {
		return false
	}
	_, write_err := os.write(file, data)
	_, newline_err := os.write_string(file, "\n")
	close_err := os.close(file)
	return write_err == nil && newline_err == nil && close_err == nil
}

dictionary_count :: proc(dictionary: ^Dictionary) -> int {
	if dictionary.entries == nil {
		return 0
	}
	return len(dictionary.entries)
}

dictionary_longest_key :: proc(dictionary: ^Dictionary) -> int {
	return dictionary.longest_key
}

dictionary_lookup_strokes :: proc(dictionary: ^Dictionary, strokes: []u64) -> (translation: string, ok: bool) {
	if dictionary.entries == nil {
		return "", false
	}

	canonical_buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
	canonical_len, canonical_ok := strokes_to_outline_key(strokes, canonical_buffer[:])
	if !canonical_ok {
		return "", false
	}

	return dictionary.entries[string(canonical_buffer[:canonical_len])]
}

dictionary_lookup_bits :: proc(dictionary: ^Dictionary, bits: u64) -> (translation: string, ok: bool) {
	strokes := [?]u64{bits}
	return dictionary_lookup_strokes(dictionary, strokes[:])
}

dictionary_lookup_stroke :: proc(dictionary: ^Dictionary, outline: string) -> (translation: string, ok: bool) {
	if dictionary.entries == nil {
		return "", false
	}

	canonical_buffer: [DICTIONARY_MAX_OUTLINE_BYTES]byte
	canonical_len, _, canonical_ok := outline_to_canonical_key(outline, canonical_buffer[:])
	if !canonical_ok {
		return "", false
	}

	return dictionary.entries[string(canonical_buffer[:canonical_len])]
}

dictionary_find_translation_outline :: proc(dictionary: ^Dictionary, translation: string, exclude_outline: string, max_stroke_count: int) -> (outline: string, ok: bool) {
	if dictionary.entries == nil {
		return "", false
	}

	best_outline := ""
	best_stroke_count := 0
	best_outline_length := 0
	for outline_key, value in dictionary.entries {
		if len(value) > 0 && value[0] == '=' {
			continue
		}
		if value != translation {
			continue
		}
		if len(exclude_outline) > 0 && outline_key == exclude_outline {
			continue
		}

		stroke_count := outline_key_stroke_count(outline_key)
		if stroke_count == 0 || (max_stroke_count > 0 && stroke_count > max_stroke_count) {
			continue
		}

		outline_length := len(outline_key)
		if len(best_outline) == 0 ||
		   stroke_count < best_stroke_count ||
		   (stroke_count == best_stroke_count && outline_length < best_outline_length) ||
		   (stroke_count == best_stroke_count && outline_length == best_outline_length && string_compare_ascii(outline_key, best_outline) < 0) {
			best_outline = outline_key
			best_stroke_count = stroke_count
			best_outline_length = outline_length
		}
	}

	if len(best_outline) == 0 {
		return "", false
	}
	return clone_string_ok(best_outline)
}
