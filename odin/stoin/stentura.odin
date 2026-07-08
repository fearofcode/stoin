package stoin

STENTURA_READ_SIZE :: 512
STENTURA_STROKES_PER_READ :: STENTURA_READ_SIZE / 4

Stentura_Action :: enum u16 {
	Open  = 0x0A,
	ReadC = 0x0B,
	Reset = 0x14,
}

STENTURA_BITS := [?]u64 {
	0,
	steno_bit(.Num),
	steno_bit(.Left_S),
	steno_bit(.Left_T),
	steno_bit(.Left_K),
	steno_bit(.Left_P),

	steno_bit(.Left_W),
	steno_bit(.Left_H),
	steno_bit(.Left_R),
	steno_bit(.A),
	steno_bit(.O),
	steno_bit(.Star),

	steno_bit(.E),
	steno_bit(.U),
	steno_bit(.Right_F),
	steno_bit(.Right_R),
	steno_bit(.Right_P),
	steno_bit(.Right_B),

	steno_bit(.Right_L),
	steno_bit(.Right_G),
	steno_bit(.Right_T),
	steno_bit(.Right_S),
	steno_bit(.Right_D),
	steno_bit(.Right_Z),
}

write_u16le :: proc(out: []byte, value: u16) {
	out[0] = byte(value & 0xFF)
	out[1] = byte(value >> 8)
}

read_u16le :: proc(data: []byte) -> u16 {
	return u16(data[0]) | (u16(data[1]) << 8)
}

stentura_crc16 :: proc(data: []byte) -> u16 {
	checksum: u16 = 0
	for b in data {
		checksum ~= u16(b)
		for _ in 0..<8 {
			if (checksum & 1) != 0 {
				checksum = (checksum >> 1) ~ 0xA001
			} else {
				checksum >>= 1
			}
		}
	}
	return checksum
}

stentura_decode_stroke :: proc(bytes: []byte) -> (bits: u64, ok: bool) {
	if len(bytes) != 4 {
		return 0, false
	}

	for byte_index in 0..<4 {
		if (bytes[byte_index] & 0xC0) != 0xC0 {
			return 0, false
		}
		for bit_index in 0..<6 {
			if (bytes[byte_index] & byte(0x20 >> uint(bit_index))) != 0 {
				bits |= STENTURA_BITS[byte_index * 6 + bit_index]
			}
		}
	}

	return bits, true
}

stentura_decode_strokes :: proc(data: []byte, out_strokes: []u64) -> (count: int, ok: bool) {
	if (len(data) % 4) != 0 {
		return 0, false
	}

	for offset := 0; offset < len(data); offset += 4 {
		bits, decoded := stentura_decode_stroke(data[offset:offset + 4])
		if !decoded {
			return 0, false
		}
		if bits == 0 {
			continue
		}
		if count >= len(out_strokes) {
			return 0, false
		}
		out_strokes[count] = bits
		count += 1
	}

	return count, true
}

stentura_make_request :: proc(
	out_packet: []byte,
	sequence: byte,
	action: Stentura_Action,
	p1: u16,
	p2: u16,
	p3: u16,
	p4: u16,
	p5: u16,
	data: []byte = nil,
) -> int {
	has_data := len(data) > 0
	packet_size := 18
	if has_data {
		packet_size += len(data) + 2
	}
	if len(out_packet) < packet_size || packet_size > 0xFFFF {
		return 0
	}

	out_packet[0] = 0x01
	out_packet[1] = sequence
	write_u16le(out_packet[2:4], u16(packet_size))
	write_u16le(out_packet[4:6], u16(action))
	write_u16le(out_packet[6:8], p1)
	write_u16le(out_packet[8:10], p2)
	write_u16le(out_packet[10:12], p3)
	write_u16le(out_packet[12:14], p4)
	write_u16le(out_packet[14:16], p5)
	write_u16le(out_packet[16:18], stentura_crc16(out_packet[1:16]))

	if has_data {
		copy(out_packet[18:18 + len(data)], data)
		write_u16le(out_packet[packet_size - 2:packet_size], stentura_crc16(data))
	}

	return packet_size
}

stentura_make_open :: proc(out_packet: []byte, sequence: byte, drive: byte, filename: string) -> int {
	return stentura_make_request(
		out_packet,
		sequence,
		.Open,
		u16(drive),
		0,
		0,
		0,
		0,
		transmute([]byte)filename,
	)
}

stentura_make_read :: proc(out_packet: []byte, sequence: byte, block: u16, byte_offset: u16, length: u16) -> int {
	return stentura_make_request(out_packet, sequence, .ReadC, 1, 0, length, block, byte_offset)
}

stentura_make_reset :: proc(out_packet: []byte, sequence: byte) -> int {
	return stentura_make_request(out_packet, sequence, .Reset, 0, 0, 0, 0, 0)
}

stentura_validate_response :: proc(packet: []byte) -> bool {
	if len(packet) < 14 || packet[0] != 0x01 {
		return false
	}

	declared_size := int(read_u16le(packet[2:4]))
	if declared_size != len(packet) {
		return false
	}

	if stentura_crc16(packet[1:12]) != read_u16le(packet[12:14]) {
		return false
	}

	if len(packet) > 14 {
		if len(packet) < 17 {
			return false
		}
		data_size := len(packet) - 16
		if stentura_crc16(packet[14:14 + data_size]) != read_u16le(packet[len(packet) - 2:len(packet)]) {
			return false
		}
	}

	return true
}
