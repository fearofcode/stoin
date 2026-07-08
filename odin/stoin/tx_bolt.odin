package stoin

Tx_Bolt :: struct {
	stroke_bits:         u64,
	queued_strokes:     [4]u64,
	queued_stroke_count: int,
	last_key_set:        int,
	had_error:           bool,
}

TX_BOLT_BITS := [4][6]u64 {
	{
		steno_bit(.Left_S),
		steno_bit(.Left_T),
		steno_bit(.Left_K),
		steno_bit(.Left_P),
		steno_bit(.Left_W),
		steno_bit(.Left_H),
	},
	{
		steno_bit(.Left_R),
		steno_bit(.A),
		steno_bit(.O),
		steno_bit(.Star),
		steno_bit(.E),
		steno_bit(.U),
	},
	{
		steno_bit(.Right_F),
		steno_bit(.Right_R),
		steno_bit(.Right_P),
		steno_bit(.Right_B),
		steno_bit(.Right_L),
		steno_bit(.Right_G),
	},
	{
		steno_bit(.Right_T),
		steno_bit(.Right_S),
		steno_bit(.Right_D),
		steno_bit(.Right_Z),
		steno_bit(.Num),
		0,
	},
}

tx_bolt_reset_stroke :: proc(tx_bolt: ^Tx_Bolt) {
	tx_bolt.stroke_bits = 0
	tx_bolt.last_key_set = 0
}

tx_bolt_enqueue_stroke :: proc(tx_bolt: ^Tx_Bolt, bits: u64) -> bool {
	if bits == 0 {
		return true
	}
	if tx_bolt.queued_stroke_count >= len(tx_bolt.queued_strokes) {
		tx_bolt.had_error = true
		return false
	}
	tx_bolt.queued_strokes[tx_bolt.queued_stroke_count] = bits
	tx_bolt.queued_stroke_count += 1
	return true
}

tx_bolt_dequeue_stroke :: proc(tx_bolt: ^Tx_Bolt) -> (bits: u64, ok: bool) {
	if tx_bolt.queued_stroke_count == 0 {
		return 0, false
	}

	bits = tx_bolt.queued_strokes[0]
	tx_bolt.queued_stroke_count -= 1
	for i in 0..<tx_bolt.queued_stroke_count {
		tx_bolt.queued_strokes[i] = tx_bolt.queued_strokes[i + 1]
	}
	return bits, true
}

tx_bolt_finish_current_stroke :: proc(tx_bolt: ^Tx_Bolt) -> bool {
	bits := tx_bolt.stroke_bits
	tx_bolt_reset_stroke(tx_bolt)
	return tx_bolt_enqueue_stroke(tx_bolt, bits)
}

tx_bolt_decode_byte :: proc(tx_bolt: ^Tx_Bolt, value: byte) -> (bits: u64, ok: bool) {
	key_set := int(value >> 6)
	if key_set <= tx_bolt.last_key_set && !tx_bolt_finish_current_stroke(tx_bolt) {
		return 0, false
	}

	tx_bolt.last_key_set = key_set
	bit_count := 6
	if key_set == 3 {
		bit_count = 5
	}
	for bit_index in 0..<bit_count {
		if ((value >> uint(bit_index)) & 1) != 0 {
			tx_bolt.stroke_bits |= TX_BOLT_BITS[key_set][bit_index]
		}
	}

	if key_set == 3 && !tx_bolt_finish_current_stroke(tx_bolt) {
		return 0, false
	}

	return tx_bolt_dequeue_stroke(tx_bolt)
}

tx_bolt_flush_stroke :: proc(tx_bolt: ^Tx_Bolt) -> (bits: u64, ok: bool) {
	if tx_bolt.queued_stroke_count == 0 && !tx_bolt_finish_current_stroke(tx_bolt) {
		return 0, false
	}
	return tx_bolt_dequeue_stroke(tx_bolt)
}

tx_bolt_has_partial_stroke :: proc(tx_bolt: ^Tx_Bolt) -> bool {
	return tx_bolt.stroke_bits != 0
}
