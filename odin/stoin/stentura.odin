package stoin

import "core:time"

STENTURA_RESPONSE_TIMEOUT_MS :: 1000
STENTURA_MAX_SEND_TRIES :: 3
STENTURA_READ_SIZE :: 512
STENTURA_STROKES_PER_READ :: STENTURA_READ_SIZE / 4
STENTURA_PACKET_BUFFER_SIZE :: 1024

Stentura_Action :: enum u16 {
	Open  = 0x0A,
	ReadC = 0x0B,
	Reset = 0x14,
}

Stentura_Read_Result :: enum {
	Ok,
	Timeout,
	Error,
}

Stentura_Status :: enum {
	Ok,
	Serial_Open_Failed,
	Build_Open_Request_Failed,
	Build_Read_Request_Failed,
	Write_Failed,
	Response_Timeout,
	Response_Error,
	Response_Sequence_Mismatch,
	Response_Action_Mismatch,
	Open_Handshake_Failed,
	Open_Handshake_Timeout,
	Initial_Drain_Failed,
	Initial_Drain_Timeout,
	Realtime_Read_Failed,
	Realtime_Response_Mismatch,
	Stroke_Decode_Failed,
	Internal_Error,
}

Stentura :: struct {
	serial:              Platform_Serial_Port,
	serial_open:         bool,
	next_sequence_value: byte,
	block:               u16,
	byte_offset:         u16,
	queued_strokes:      [STENTURA_STROKES_PER_READ]u64,
	queued_stroke_count: int,
	had_error:           bool,
	status:              Stentura_Status,
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

stentura_monotonic_ms :: proc() -> u64 {
	tick := time.tick_now()
	return u64(tick._nsec / 1_000_000)
}

stentura_next_sequence :: proc(stentura: ^Stentura) -> byte {
	sequence := stentura.next_sequence_value
	stentura.next_sequence_value += 1
	return sequence
}

stentura_set_status :: proc(stentura: ^Stentura, status: Stentura_Status) {
	if stentura != nil {
		stentura.status = status
	}
}

stentura_status_message :: proc(stentura: ^Stentura) -> string {
	if stentura == nil {
		return "unknown Stentura error"
	}

	switch stentura.status {
	case .Ok:
		return ""
	case .Serial_Open_Failed:
		return "serial open failed"
	case .Build_Open_Request_Failed:
		return "failed to build OPEN request"
	case .Build_Read_Request_Failed:
		return "failed to build READ request"
	case .Write_Failed:
		return "serial write failed"
	case .Response_Timeout:
		return "timed out waiting for response"
	case .Response_Error:
		return "invalid or unreadable response"
	case .Response_Sequence_Mismatch:
		return "response sequence mismatch"
	case .Response_Action_Mismatch:
		return "response action mismatch"
	case .Open_Handshake_Failed:
		return "OPEN handshake failed"
	case .Open_Handshake_Timeout:
		return "OPEN handshake timed out"
	case .Initial_Drain_Failed:
		return "initial realtime drain failed"
	case .Initial_Drain_Timeout:
		return "initial realtime drain timed out"
	case .Realtime_Read_Failed:
		return "realtime read failed"
	case .Realtime_Response_Mismatch:
		return "realtime response size mismatch"
	case .Stroke_Decode_Failed:
		return "stroke decode failed"
	case .Internal_Error:
		return "internal Stentura error"
	}

	return "unknown Stentura error"
}

stentura_close_with_status :: proc(stentura: ^Stentura, status: Stentura_Status) {
	stentura_close(stentura)
	stentura_set_status(stentura, status)
}

stentura_dequeue_stroke :: proc(stentura: ^Stentura) -> (bits: u64, ok: bool) {
	if stentura == nil || stentura.queued_stroke_count == 0 {
		return 0, false
	}
	bits = stentura.queued_strokes[0]
	stentura.queued_stroke_count -= 1
	for i in 0..<stentura.queued_stroke_count {
		stentura.queued_strokes[i] = stentura.queued_strokes[i + 1]
	}
	return bits, true
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

stentura_read_bytes :: proc(stentura: ^Stentura, out_bytes: []byte, timeout_ms: uint) -> Stentura_Read_Result {
	if stentura == nil || !stentura.serial_open {
		if stentura != nil {
			stentura.had_error = true
			stentura.status = .Internal_Error
		}
		return .Error
	}

	started_ms := stentura_monotonic_ms()
	deadline_ms := started_ms + u64(timeout_ms)
	for i in 0..<len(out_bytes) {
		now_ms := stentura_monotonic_ms()
		if timeout_ms != 0 && now_ms >= deadline_ms {
			return .Timeout
		}
		remaining_ms := timeout_ms
		if timeout_ms != 0 {
			remaining := deadline_ms - now_ms
			if remaining > u64(max(uint)) {
				remaining_ms = max(uint)
			} else {
				remaining_ms = uint(remaining)
			}
		}

		value, read_result := platform_serial_read_byte(&stentura.serial, remaining_ms)
		switch read_result {
		case .Byte:
			out_bytes[i] = value
		case .None:
			return .Timeout
		case .Error:
			stentura.had_error = true
			stentura.status = .Response_Error
			return .Error
		}
	}

	return .Ok
}

stentura_read_packet :: proc(stentura: ^Stentura, out_packet: []byte) -> (packet_size: int, result: Stentura_Read_Result) {
	if len(out_packet) < 14 {
		if stentura != nil {
			stentura.had_error = true
			stentura.status = .Internal_Error
		}
		return 0, .Error
	}

	header_result := stentura_read_bytes(stentura, out_packet[:4], STENTURA_RESPONSE_TIMEOUT_MS)
	if header_result != .Ok {
		return 0, header_result
	}

	declared_size := int(read_u16le(out_packet[2:4]))
	if declared_size < 14 || declared_size > len(out_packet) {
		stentura.had_error = true
		stentura.status = .Response_Error
		return 0, .Error
	}

	body_result := stentura_read_bytes(stentura, out_packet[4:declared_size], STENTURA_RESPONSE_TIMEOUT_MS)
	if body_result != .Ok {
		return 0, body_result
	}

	if !stentura_validate_response(out_packet[:declared_size]) {
		stentura.had_error = true
		stentura.status = .Response_Error
		return 0, .Error
	}

	return declared_size, .Ok
}

stentura_send_receive :: proc(stentura: ^Stentura, request: []byte, response: []byte) -> (response_size: int, ok: bool) {
	if stentura == nil || !stentura.serial_open || len(request) < 6 || len(response) < 14 {
		if stentura != nil {
			stentura.had_error = true
			stentura.status = .Internal_Error
		}
		return 0, false
	}

	request_sequence := request[1]
	request_action := read_u16le(request[4:6])
	last_status := Stentura_Status.Response_Timeout
	for _ in 0..<STENTURA_MAX_SEND_TRIES {
		if !platform_serial_write_all(&stentura.serial, request, STENTURA_RESPONSE_TIMEOUT_MS) {
			stentura.had_error = true
			stentura.status = .Write_Failed
			return 0, false
		}

		received_size, read_result := stentura_read_packet(stentura, response)
		if read_result == .Timeout {
			last_status = .Response_Timeout
			continue
		}
		if read_result != .Ok {
			stentura.had_error = true
			if stentura.status == .Ok {
				stentura.status = .Response_Error
			}
			return 0, false
		}
		if response[1] != request_sequence {
			last_status = .Response_Sequence_Mismatch
			continue
		}
		if read_u16le(response[4:6]) != request_action {
			stentura.had_error = true
			stentura.status = .Response_Action_Mismatch
			return 0, false
		}

		stentura.status = .Ok
		return received_size, true
	}

	stentura.had_error = true
	stentura.status = last_status
	return 0, false
}

stentura_read_realtime_data :: proc(stentura: ^Stentura, out_data: []byte) -> (read_size: int, ok: bool) {
	if stentura == nil || len(out_data) < STENTURA_READ_SIZE {
		if stentura != nil {
			stentura.had_error = true
			stentura.status = .Internal_Error
		}
		return 0, false
	}

	request: [32]byte
	response: [STENTURA_PACKET_BUFFER_SIZE]byte
	request_size := stentura_make_read(
		request[:],
		stentura_next_sequence(stentura),
		stentura.block,
		stentura.byte_offset,
		STENTURA_READ_SIZE,
	)
	if request_size == 0 {
		stentura.had_error = true
		stentura.status = .Build_Read_Request_Failed
		return 0, false
	}

	response_size, received := stentura_send_receive(stentura, request[:request_size], response[:])
	if !received {
		if stentura.status == .Ok {
			stentura.status = .Realtime_Read_Failed
		}
		return 0, false
	}

	p1 := read_u16le(response[8:10])
	response_data_size := 0
	if response_size > 14 {
		response_data_size = response_size - 16
	}
	if !((p1 == 0 && response_size == 14) || int(p1) == response_data_size) ||
	   response_data_size > len(out_data) {
		stentura.had_error = true
		stentura.status = .Realtime_Response_Mismatch
		return 0, false
	}

	if response_data_size > 0 {
		copy(out_data[:response_data_size], response[14:14 + response_data_size])
	}

	next_byte := u32(stentura.byte_offset) + u32(p1)
	stentura.block = u16(u32(stentura.block) + (next_byte / STENTURA_READ_SIZE))
	stentura.byte_offset = u16(next_byte % STENTURA_READ_SIZE)
	return response_data_size, true
}

stentura_drain_realtime_file :: proc(stentura: ^Stentura) -> bool {
	stroke_data: [STENTURA_READ_SIZE]byte
	for {
		read_size, ok := stentura_read_realtime_data(stentura, stroke_data[:])
		if !ok {
			return false
		}
		if read_size == 0 {
			return true
		}
	}
}

stentura_open :: proc(stentura: ^Stentura, port_path: string, baud_rate: int) -> bool {
	if stentura == nil {
		return false
	}
	stentura^ = {}
	resolved_baud_rate := baud_rate
	if resolved_baud_rate == 0 {
		resolved_baud_rate = PLATFORM_SERIAL_DEFAULT_BAUD
	}
	if !platform_serial_open(&stentura.serial, port_path, resolved_baud_rate) {
		stentura.status = .Serial_Open_Failed
		return false
	}
	stentura.serial_open = true

	time.sleep(time.Duration(STENTURA_RESPONSE_TIMEOUT_MS) * time.Millisecond)
	platform_serial_flush(&stentura.serial)

	request: [64]byte
	response: [STENTURA_PACKET_BUFFER_SIZE]byte
	request_size := stentura_make_open(request[:], stentura_next_sequence(stentura), 'A', "REALTIME.000")
	if request_size == 0 {
		stentura_close_with_status(stentura, .Build_Open_Request_Failed)
		return false
	}

	_, ok := stentura_send_receive(stentura, request[:request_size], response[:])
	if !ok {
		status := stentura.status
		if status == .Response_Timeout {
			status = .Open_Handshake_Timeout
		} else if status == .Ok {
			status = .Open_Handshake_Failed
		}
		stentura_close_with_status(stentura, status)
		return false
	}
	if !stentura_drain_realtime_file(stentura) {
		status := stentura.status
		if status == .Response_Timeout {
			status = .Initial_Drain_Timeout
		} else if status == .Ok {
			status = .Initial_Drain_Failed
		}
		stentura_close_with_status(stentura, status)
		return false
	}

	stentura.status = .Ok
	return true
}

stentura_close :: proc(stentura: ^Stentura) {
	if stentura == nil {
		return
	}
	if stentura.serial_open {
		platform_serial_close(&stentura.serial)
	}
	stentura^ = {}
}

stentura_port_path :: proc(stentura: ^Stentura) -> string {
	if stentura == nil || !stentura.serial_open {
		return ""
	}
	return stentura.serial.port_path
}

stentura_had_error :: proc(stentura: ^Stentura) -> bool {
	return stentura != nil && (stentura.had_error || (stentura.serial_open && platform_serial_had_error(&stentura.serial)))
}

stentura_read_stroke :: proc(stentura: ^Stentura) -> (bits: u64, ok: bool) {
	if stentura == nil || !stentura.serial_open {
		return 0, false
	}

	if bits, dequeued := stentura_dequeue_stroke(stentura); dequeued {
		return bits, true
	}

	stroke_data: [STENTURA_READ_SIZE]byte
	read_size, read_ok := stentura_read_realtime_data(stentura, stroke_data[:])
	if !read_ok || read_size == 0 {
		return 0, false
	}

	decoded_count, decoded := stentura_decode_strokes(stroke_data[:read_size], stentura.queued_strokes[:])
	if !decoded {
		stentura.had_error = true
		stentura.status = .Stroke_Decode_Failed
		return 0, false
	}
	stentura.queued_stroke_count = decoded_count
	return stentura_dequeue_stroke(stentura)
}
