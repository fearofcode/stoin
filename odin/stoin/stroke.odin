package stoin

Steno_Key :: enum int {
	Num,
	Left_S,
	Left_T,
	Left_K,
	Left_P,
	Left_W,
	Left_H,
	Left_R,
	A,
	O,
	Star,
	E,
	U,
	Right_F,
	Right_R,
	Right_P,
	Right_B,
	Right_L,
	Right_G,
	Right_T,
	Right_S,
	Right_D,
	Right_Z,
}

Stroke_Region :: enum {
	Left,
	Vowel,
	Right,
}

Stroke_Label :: struct {
	bit:   u64,
	label: byte,
}

steno_bit :: proc "contextless" (key: Steno_Key) -> u64 {
	return u64(1) << u64(key)
}

left_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case '#':
		return steno_bit(.Num)
	case 'S':
		return steno_bit(.Left_S)
	case 'T':
		return steno_bit(.Left_T)
	case 'K':
		return steno_bit(.Left_K)
	case 'P':
		return steno_bit(.Left_P)
	case 'W':
		return steno_bit(.Left_W)
	case 'H':
		return steno_bit(.Left_H)
	case 'R':
		return steno_bit(.Left_R)
	}
	return 0
}

left_number_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case '1':
		return steno_bit(.Left_S)
	case '2':
		return steno_bit(.Left_T)
	case '3':
		return steno_bit(.Left_P)
	case '4':
		return steno_bit(.Left_H)
	}
	return 0
}

vowel_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case 'A':
		return steno_bit(.A)
	case 'O':
		return steno_bit(.O)
	case '*':
		return steno_bit(.Star)
	case 'E':
		return steno_bit(.E)
	case 'U':
		return steno_bit(.U)
	}
	return 0
}

vowel_number_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case '5':
		return steno_bit(.A)
	case '0':
		return steno_bit(.O)
	}
	return 0
}

right_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case 'F':
		return steno_bit(.Right_F)
	case 'R':
		return steno_bit(.Right_R)
	case 'P':
		return steno_bit(.Right_P)
	case 'B':
		return steno_bit(.Right_B)
	case 'L':
		return steno_bit(.Right_L)
	case 'G':
		return steno_bit(.Right_G)
	case 'T':
		return steno_bit(.Right_T)
	case 'S':
		return steno_bit(.Right_S)
	case 'D':
		return steno_bit(.Right_D)
	case 'Z':
		return steno_bit(.Right_Z)
	}
	return 0
}

right_number_bit_for_char :: proc(c: byte) -> u64 {
	switch c {
	case '6':
		return steno_bit(.Right_F)
	case '7':
		return steno_bit(.Right_P)
	case '8':
		return steno_bit(.Right_L)
	case '9':
		return steno_bit(.Right_T)
	}
	return 0
}

add_steno_bit :: proc(bits: ^u64, bit: u64) -> bool {
	if bit == 0 || (bits^ & bit) != 0 {
		return false
	}
	bits^ |= bit
	return true
}

stroke_string_to_bits :: proc(stroke: string) -> (bits: u64, ok: bool) {
	region := Stroke_Region.Left
	saw_any := false
	saw_number_digit := false

	for i in 0..<len(stroke) {
		c := stroke[i]
		bit: u64 = 0

		if c == '/' {
			return 0, false
		}
		if c == '-' {
			region = .Right
			continue
		}

		switch region {
		case .Left:
			bit = left_bit_for_char(c)
			if bit == 0 {
				bit = left_number_bit_for_char(c)
				if bit != 0 {
					saw_number_digit = true
				}
			}
			if bit == 0 {
				bit = vowel_bit_for_char(c)
				if bit != 0 {
					region = .Vowel
				}
			}
			if bit == 0 {
				bit = vowel_number_bit_for_char(c)
				if bit != 0 {
					region = .Vowel
					saw_number_digit = true
				}
			}
			if bit == 0 {
				bit = right_bit_for_char(c)
				if bit != 0 {
					region = .Right
				}
			}
			if bit == 0 {
				bit = right_number_bit_for_char(c)
				if bit != 0 {
					region = .Right
					saw_number_digit = true
				}
			}
		case .Vowel:
			bit = vowel_bit_for_char(c)
			if bit == 0 {
				bit = vowel_number_bit_for_char(c)
				if bit != 0 {
					saw_number_digit = true
				}
			}
			if bit == 0 {
				bit = right_bit_for_char(c)
				if bit != 0 {
					region = .Right
				}
			}
			if bit == 0 {
				bit = right_number_bit_for_char(c)
				if bit != 0 {
					region = .Right
					saw_number_digit = true
				}
			}
		case .Right:
			bit = right_bit_for_char(c)
			if bit == 0 {
				bit = right_number_bit_for_char(c)
				if bit != 0 {
					saw_number_digit = true
				}
			}
		}

		if !add_steno_bit(&bits, bit) {
			return 0, false
		}
		saw_any = true
	}

	if !saw_any || (saw_number_digit && (bits & steno_bit(.Num)) == 0) {
		return 0, false
	}

	return bits, true
}

append_byte_to_buffer :: proc(out: []byte, index: ^int, c: byte) -> bool {
	if index^ >= len(out) {
		return false
	}
	out[index^] = c
	index^ += 1
	return true
}

chord_bits_to_string :: proc(bits: u64, out: []byte) -> (n: int, ok: bool) {
	index := 0

	left_and_vowels := [?]Stroke_Label {
		{bit = steno_bit(.Num), label = '#'},
		{bit = steno_bit(.Left_S), label = 'S'},
		{bit = steno_bit(.Left_T), label = 'T'},
		{bit = steno_bit(.Left_K), label = 'K'},
		{bit = steno_bit(.Left_P), label = 'P'},
		{bit = steno_bit(.Left_W), label = 'W'},
		{bit = steno_bit(.Left_H), label = 'H'},
		{bit = steno_bit(.Left_R), label = 'R'},
		{bit = steno_bit(.A), label = 'A'},
		{bit = steno_bit(.O), label = 'O'},
		{bit = steno_bit(.Star), label = '*'},
		{bit = steno_bit(.E), label = 'E'},
		{bit = steno_bit(.U), label = 'U'},
	}
	right := [?]Stroke_Label {
		{bit = steno_bit(.Right_F), label = 'F'},
		{bit = steno_bit(.Right_R), label = 'R'},
		{bit = steno_bit(.Right_P), label = 'P'},
		{bit = steno_bit(.Right_B), label = 'B'},
		{bit = steno_bit(.Right_L), label = 'L'},
		{bit = steno_bit(.Right_G), label = 'G'},
		{bit = steno_bit(.Right_T), label = 'T'},
		{bit = steno_bit(.Right_S), label = 'S'},
		{bit = steno_bit(.Right_D), label = 'D'},
		{bit = steno_bit(.Right_Z), label = 'Z'},
	}

	for item in left_and_vowels {
		if (bits & item.bit) != 0 && !append_byte_to_buffer(out, &index, item.label) {
			return 0, false
		}
	}

	right_bits: u64 = 0
	for item in right {
		right_bits |= item.bit
	}

	if (bits & right_bits) != 0 {
		right_labels: [16]byte
		right_index := 0
		for item in right {
			if (bits & item.bit) != 0 && !append_byte_to_buffer(right_labels[:], &right_index, item.label) {
				return 0, false
			}
		}

		implicit_hyphen: [64]byte
		implicit_index := 0
		implicit_ok := true
		for i in 0..<index {
			implicit_ok = implicit_ok && append_byte_to_buffer(implicit_hyphen[:], &implicit_index, out[i])
		}
		for i in 0..<right_index {
			implicit_ok = implicit_ok && append_byte_to_buffer(implicit_hyphen[:], &implicit_index, right_labels[i])
		}
		if implicit_ok {
			implicit_bits, implicit_parse_ok := stroke_string_to_bits(string(implicit_hyphen[:implicit_index]))
			if implicit_parse_ok && implicit_bits == bits {
				for i in 0..<right_index {
					if !append_byte_to_buffer(out, &index, right_labels[i]) {
						return 0, false
					}
				}
				return index, true
			}
		}

		if !append_byte_to_buffer(out, &index, '-') {
			return 0, false
		}
		for i in 0..<right_index {
			if !append_byte_to_buffer(out, &index, right_labels[i]) {
				return 0, false
			}
		}
	}

	return index, index > 0
}
