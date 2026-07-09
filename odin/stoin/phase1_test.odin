package stoin

import "core:bytes"
import "core:testing"

expect_outline :: proc(t: ^testing.T, name: string, bits: u64, expected: string) {
	buffer: [64]byte
	n, ok := chord_bits_to_string(bits, buffer[:])
	testing.expectf(t, ok, "%s: outline formatting failed", name)
	if ok {
		testing.expectf(t, string(buffer[:n]) == expected, "%s: expected %s, got %s", name, expected, string(buffer[:n]))
	}
}

expect_stroke_format :: proc(t: ^testing.T, input: string, expected: string) {
	bits, parsed := stroke_string_to_bits(input)
	testing.expectf(t, parsed, "expected %s to parse", input)
	if parsed {
		expect_outline(t, input, bits, expected)
	}
}

@(test)
test_stroke_parse_and_format :: proc(t: ^testing.T) {
	rr_bits, ok := stroke_string_to_bits("R-R")
	testing.expect(t, ok)
	expect_outline(t, "R-R canonicalization", rr_bits, "R-R")

	expect_stroke_format(t, "SA-P", "SAP")
	expect_stroke_format(t, "TAT", "TAT")
	expect_stroke_format(t, "R-R", "R-R")
	expect_stroke_format(t, "-T", "-T")
	expect_stroke_format(t, "-F", "F")
	expect_stroke_format(t, "#1", "#S")
	expect_stroke_format(t, "#2", "#T")
	expect_stroke_format(t, "#3", "#P")
	expect_stroke_format(t, "#4", "#H")
	expect_stroke_format(t, "#5", "#A")
	expect_stroke_format(t, "#0", "#O")
	expect_stroke_format(t, "#-6", "#F")
	expect_stroke_format(t, "#-7", "#-P")
	expect_stroke_format(t, "#-8", "#L")
	expect_stroke_format(t, "#-9", "#-T")
	expect_stroke_format(t, "#*-678G", "#*FPLG")

	_, bare_number_ok := stroke_string_to_bits("1")
	testing.expect(t, !bare_number_ok)

	drill_chords := [?]string {
		"SAP", "HUD", "SOG", "TOD", "WET", "POG", "ROD", "KUS", "PEB", "ROR",
		"WEZ", "WEL", "TER", "TAT", "WEF", "KAB", "WES", "SAP", "TAS", "RET",
		"TAD", "PEP", "SEB", "KOF", "TUZ", "PEF", "HEL", "PUB", "RAT", "WAF",
		"TAB", "RAS", "HUP", "WUP", "PEZ", "SOF", "HUR", "PUZ", "SOB", "POT",
		"KED", "WUD", "SAG", "RAP", "RAL", "ROL", "WOZ", "KAD", "KAT", "KOB",
		"RAD", "TAR", "SAL", "ROF", "SOR", "WOT", "HUF", "TUR", "KAF", "HOR",
		"SOD", "KOT", "SEF", "RED", "HAP", "PAP", "KEG", "KOZ", "TUS", "SOZ",
		"TAG", "HAS", "TAF", "HES", "HOL", "WUR", "TEB", "HAB", "HER", "PER",
		"TOP", "HAZ", "POL", "WOS", "HOP", "SUT", "TOR", "REL", "PAT", "SER",
		"WUS", "PUP", "KAG", "POD", "SUB", "HED", "SAB", "SUL", "TEF", "SOL",
	}
	for chord in drill_chords {
		expect_stroke_format(t, chord, chord)
	}
}

@(test)
test_gemini_pr_decode_packet :: proc(t: ^testing.T) {
	gemini_sat := [?]byte{0x80, 0x40, 0x20, 0x00, 0x04, 0x00}
	bits, ok := gemini_pr_decode_packet(gemini_sat[:])
	testing.expect(t, ok)
	expect_outline(t, "Gemini PR SAT packet", bits, "SAT")

	gemini_number_star_z := [?]byte{0xA0, 0x00, 0x08, 0x00, 0x00, 0x01}
	bits, ok = gemini_pr_decode_packet(gemini_number_star_z[:])
	testing.expect(t, ok)
	expect_outline(t, "Gemini PR number star Z packet", bits, "#*Z")

	bad_gemini_start := [?]byte{0x00, 0x40, 0x20, 0x00, 0x04, 0x00}
	_, ok = gemini_pr_decode_packet(bad_gemini_start[:])
	testing.expect(t, !ok)

	bad_gemini_continuation := [?]byte{0x80, 0xC0, 0x20, 0x00, 0x04, 0x00}
	_, ok = gemini_pr_decode_packet(bad_gemini_continuation[:])
	testing.expect(t, !ok)

	gemini: Gemini_Pr
	stream := [?]byte{0x00, 0x80, 0xC0, 0x80, 0x40, 0x20, 0x00, 0x04, 0x00}
	decoded := false
	for value in stream {
		bits, ok = gemini_pr_decode_byte(&gemini, value)
		if ok {
			decoded = true
			expect_outline(t, "Gemini PR stream resync", bits, "SAT")
		}
	}
	testing.expect(t, decoded)
}

@(test)
test_stentura_helpers :: proc(t: ^testing.T) {
	crc_input := transmute([]byte)string("123456789")
	testing.expect_value(t, stentura_crc16(crc_input), u16(0xBB3D))

	stentura_sat := [?]byte{0xC8, 0xC4, 0xC0, 0xC8}
	bits, ok := stentura_decode_stroke(stentura_sat[:])
	testing.expect(t, ok)
	expect_outline(t, "Stentura SAT stroke", bits, "SAT")

	stentura_praoerbgs := [?]byte{0xC1, 0xCE, 0xE5, 0xD4}
	bits, ok = stentura_decode_stroke(stentura_praoerbgs[:])
	testing.expect(t, ok)
	expect_outline(t, "Stentura PRAOERBGS stroke", bits, "PRAOERBGS")

	bad_stentura_stroke := [?]byte{0xC8, 0x84, 0xC0, 0xC8}
	_, ok = stentura_decode_stroke(bad_stentura_stroke[:])
	testing.expect(t, !ok)

	stentura_strokes := [?]byte{
		0xC8, 0xC4, 0xC0, 0xC8,
		0xC1, 0xCE, 0xE5, 0xD4,
	}
	decoded_stentura_strokes: [2]u64
	decoded_count, decoded := stentura_decode_strokes(stentura_strokes[:], decoded_stentura_strokes[:])
	testing.expect(t, decoded)
	testing.expect_value(t, decoded_count, 2)

	stentura_packet: [64]byte
	expected_stentura_read := [?]byte{
		0x01, 0x20, 0x12, 0x00, 0x0B, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x14, 0x00, 0x01, 0x00, 0x08, 0x00, 0x83, 0x3C,
	}
	packet_size := stentura_make_read(stentura_packet[:], 32, 1, 8, 20)
	testing.expectf(t, packet_size == len(expected_stentura_read), "Stentura READC request size mismatch")
	testing.expectf(t, bytes.equal(stentura_packet[:packet_size], expected_stentura_read[:]), "Stentura READC request bytes mismatch")

	expected_stentura_open := [?]byte{
		0x01, 0x4F, 0x20, 0x00, 0x0A, 0x00, 0x41, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xDD,
		0x52, 0x45, 0x41, 0x4C, 0x54, 0x49, 0x4D, 0x45, 0x2E,
		0x30, 0x30, 0x30, 0x49, 0xF2,
	}
	packet_size = stentura_make_open(stentura_packet[:], 79, 'A', "REALTIME.000")
	testing.expectf(t, packet_size == len(expected_stentura_open), "Stentura OPEN request size mismatch")
	testing.expectf(t, bytes.equal(stentura_packet[:packet_size], expected_stentura_open[:]), "Stentura OPEN request bytes mismatch")

	expected_stentura_reset := [?]byte{
		0x01, 0x43, 0x12, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x13,
	}
	packet_size = stentura_make_reset(stentura_packet[:], 67)
	testing.expectf(t, packet_size == len(expected_stentura_reset), "Stentura RESET request size mismatch")
	testing.expectf(t, bytes.equal(stentura_packet[:packet_size], expected_stentura_reset[:]), "Stentura RESET request bytes mismatch")

	valid_stentura_response := [?]byte{
		0x01, 0x05, 0x0E, 0x00, 0x09, 0x00, 0x01,
		0x00, 0x02, 0x00, 0x03, 0x00, 0xB0, 0xCA,
	}
	valid_stentura_data_response := [?]byte{
		0x01, 0x05, 0x15, 0x00, 0x09, 0x00, 0x01, 0x00, 0x02,
		0x00, 0x03, 0x00, 0xC0, 0xBA, 0x68, 0x65, 0x6C, 0x6C,
		0x6F, 0xD2, 0x34,
	}
	testing.expect(t, stentura_validate_response(valid_stentura_response[:]))
	testing.expect(t, stentura_validate_response(valid_stentura_data_response[:]))
	testing.expect(t, !stentura_validate_response(valid_stentura_response[:len(valid_stentura_response) - 1]))

	bad_stentura_response := valid_stentura_data_response
	bad_stentura_response[len(bad_stentura_response) - 1] ~= 0x01
	testing.expect(t, !stentura_validate_response(bad_stentura_response[:]))
}

@(test)
test_tx_bolt_decode_byte :: proc(t: ^testing.T) {
	tx_bolt: Tx_Bolt
	bits: u64
	ok: bool

	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x01)
	testing.expect(t, !ok)
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x42)
	testing.expect(t, !ok)
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x84)
	testing.expect(t, !ok)
	bits, ok = tx_bolt_flush_stroke(&tx_bolt)
	testing.expect(t, ok)
	expect_outline(t, "TX Bolt SAP packet", bits, "SAP")

	tx_bolt = {}
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x48)
	testing.expect(t, !ok)
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0xD8)
	testing.expect(t, ok)
	expect_outline(t, "TX Bolt number star Z packet", bits, "#*Z")

	Tx_Bolt_Number_Bar_Case :: struct {
		bytes:      [3]byte,
		byte_count: int,
		expected:   string,
	}
	number_bar_cases := [?]Tx_Bolt_Number_Bar_Case {
		{bytes = {0x01, 0xD0, 0x00}, byte_count = 2, expected = "#S"},
		{bytes = {0x02, 0xD0, 0x00}, byte_count = 2, expected = "#T"},
		{bytes = {0x08, 0xD0, 0x00}, byte_count = 2, expected = "#P"},
		{bytes = {0x20, 0xD0, 0x00}, byte_count = 2, expected = "#H"},
		{bytes = {0x00, 0x42, 0xD0}, byte_count = 3, expected = "#A"},
		{bytes = {0x44, 0xD0, 0x00}, byte_count = 2, expected = "#O"},
		{bytes = {0x81, 0xD0, 0x00}, byte_count = 2, expected = "#F"},
		{bytes = {0x84, 0xD0, 0x00}, byte_count = 2, expected = "#-P"},
		{bytes = {0x90, 0xD0, 0x00}, byte_count = 2, expected = "#L"},
		{bytes = {0xD1, 0x00, 0x00}, byte_count = 1, expected = "#-T"},
	}
	for c in number_bar_cases {
		tx_bolt = {}
		decoded := false
		for i in 0..<c.byte_count {
			bits, decoded = tx_bolt_decode_byte(&tx_bolt, c.bytes[i])
		}
		testing.expect(t, decoded)
		expect_outline(t, "TX Bolt number-bar position", bits, c.expected)
	}

	tx_bolt = {}
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x01)
	testing.expect(t, !ok)
	bits, ok = tx_bolt_decode_byte(&tx_bolt, 0x02)
	testing.expect(t, ok)
	expect_outline(t, "TX Bolt lower set starts new stroke", bits, "S")
	bits, ok = tx_bolt_flush_stroke(&tx_bolt)
	testing.expect(t, ok)
	expect_outline(t, "TX Bolt queued next stroke", bits, "T")
}

@(test)
test_stroke_merge :: proc(t: ^testing.T) {
	merge: Stroke_Merge
	stroke_merge_init(&merge, 150)
	defer stroke_merge_destroy(&merge)

	testing.expect(t, stroke_merge_push(&merge, 1, 0x01, 1000))
	ok: bool
	output: u64
	_, ok = stroke_merge_next_output(&merge)
	testing.expect(t, !ok)
	testing.expect(t, stroke_merge_push(&merge, 2, 0x20, 1030))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x21))
	_, ok = stroke_merge_next_output(&merge)
	testing.expect(t, !ok)

	stroke_merge_clear(&merge)
	testing.expect(t, stroke_merge_push(&merge, 1, 0x01, 2000))
	testing.expect(t, stroke_merge_push(&merge, 1, 0x02, 2010))
	_, ok = stroke_merge_next_output(&merge)
	testing.expect(t, !ok)
	testing.expect(t, stroke_merge_push(&merge, 2, 0x20, 2020))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x21))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x02))

	stroke_merge_clear(&merge)
	testing.expect(t, stroke_merge_push(&merge, 1, 0x04, 3000))
	testing.expect(t, stroke_merge_push(&merge, 1, 0x08, 3010))
	testing.expect(t, stroke_merge_poll(&merge, 3150))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x04))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x08))

	stroke_merge_clear(&merge)
	stroke_merge_set_window_ms(&merge, 0)
	testing.expect(t, stroke_merge_push(&merge, 1, 0x10, 4000))
	output, ok = stroke_merge_next_output(&merge)
	testing.expect(t, ok)
	testing.expect_value(t, output, u64(0x10))
}
