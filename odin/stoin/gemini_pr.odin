package stoin

GEMINI_PR_PACKET_SIZE :: 6

Gemini_Pr :: struct {
	packet:       [GEMINI_PR_PACKET_SIZE]byte,
	packet_index: int,
}

GEMINI_PR_BITS := [?]u64 {
	0, steno_bit(.Num), steno_bit(.Num), steno_bit(.Num),
	steno_bit(.Num), steno_bit(.Num), steno_bit(.Num),

	steno_bit(.Left_S), steno_bit(.Left_S), steno_bit(.Left_T),
	steno_bit(.Left_K), steno_bit(.Left_P), steno_bit(.Left_W),
	steno_bit(.Left_H),

	steno_bit(.Left_R), steno_bit(.A), steno_bit(.O),
	steno_bit(.Star), steno_bit(.Star), 0, 0,

	0, steno_bit(.Star), steno_bit(.Star), steno_bit(.E),
	steno_bit(.U), steno_bit(.Right_F), steno_bit(.Right_R),

	steno_bit(.Right_P), steno_bit(.Right_B), steno_bit(.Right_L),
	steno_bit(.Right_G), steno_bit(.Right_T), steno_bit(.Right_S),
	steno_bit(.Right_D),

	steno_bit(.Num), steno_bit(.Num), steno_bit(.Num),
	steno_bit(.Num), steno_bit(.Num), steno_bit(.Num),
	steno_bit(.Right_Z),
}

gemini_pr_decode_packet :: proc(packet: []byte) -> (bits: u64, ok: bool) {
	if len(packet) != GEMINI_PR_PACKET_SIZE {
		return 0, false
	}
	if (packet[0] & 0x80) == 0 {
		return 0, false
	}
	for i in 1..<GEMINI_PR_PACKET_SIZE {
		if (packet[i] & 0x80) != 0 {
			return 0, false
		}
	}

	for byte_index in 0..<GEMINI_PR_PACKET_SIZE {
		for bit_index in 0..<7 {
			if (packet[byte_index] & byte(0x40 >> uint(bit_index))) != 0 {
				bits |= GEMINI_PR_BITS[byte_index * 7 + bit_index]
			}
		}
	}

	if bits == 0 {
		return 0, false
	}

	return bits, true
}

gemini_pr_decode_byte :: proc(gemini: ^Gemini_Pr, value: byte) -> (bits: u64, ok: bool) {
	if gemini == nil {
		return 0, false
	}

	if gemini.packet_index == 0 {
		if (value & 0x80) == 0 {
			return 0, false
		}
		gemini.packet[0] = value
		gemini.packet_index = 1
		return 0, false
	}

	if (value & 0x80) != 0 {
		gemini.packet[0] = value
		gemini.packet_index = 1
		return 0, false
	}

	gemini.packet[gemini.packet_index] = value
	gemini.packet_index += 1
	if gemini.packet_index != GEMINI_PR_PACKET_SIZE {
		return 0, false
	}

	gemini.packet_index = 0
	return gemini_pr_decode_packet(gemini.packet[:])
}
