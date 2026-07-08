package stoin

Stroke_Merge :: struct {
	pending_bits:          u64,
	pending_deadline_ms:   u64,
	queued_after_pending: [dynamic]u64,
	outputs:              [dynamic]u64,
	pending_source_id:     int,
	window_ms:             uint,
	has_pending:           bool,
}

time_reached :: proc(now_ms: u64, deadline_ms: u64) -> bool {
	return i64(now_ms - deadline_ms) >= 0
}

stroke_merge_init :: proc(merge: ^Stroke_Merge, window_ms: uint) {
	merge^ = {}
	merge.window_ms = window_ms
	merge.queued_after_pending = make([dynamic]u64)
	merge.outputs = make([dynamic]u64)
}

stroke_merge_destroy :: proc(merge: ^Stroke_Merge) {
	delete(merge.queued_after_pending)
	delete(merge.outputs)
	merge^ = {}
}

stroke_merge_clear_pending_only :: proc(merge: ^Stroke_Merge) {
	merge.pending_bits = 0
	merge.pending_deadline_ms = 0
	merge.pending_source_id = 0
	merge.has_pending = false
	clear(&merge.queued_after_pending)
}

stroke_merge_queue_output :: proc(merge: ^Stroke_Merge, bits: u64) {
	if bits == 0 {
		return
	}
	append(&merge.outputs, bits)
}

stroke_merge_flush_pending :: proc(merge: ^Stroke_Merge) {
	if !merge.has_pending {
		return
	}

	stroke_merge_queue_output(merge, merge.pending_bits)
	for bits in merge.queued_after_pending {
		stroke_merge_queue_output(merge, bits)
	}
	stroke_merge_clear_pending_only(merge)
}

stroke_merge_clear :: proc(merge: ^Stroke_Merge) {
	stroke_merge_clear_pending_only(merge)
	clear(&merge.outputs)
}

stroke_merge_set_window_ms :: proc(merge: ^Stroke_Merge, window_ms: uint) {
	merge.window_ms = window_ms
	if window_ms == 0 {
		stroke_merge_flush_pending(merge)
	}
}

stroke_merge_push :: proc(merge: ^Stroke_Merge, source_id: int, bits: u64, now_ms: u64) -> bool {
	if bits == 0 {
		return true
	}

	stroke_merge_poll(merge, now_ms)
	if merge.window_ms == 0 {
		stroke_merge_queue_output(merge, bits)
		return true
	}

	if !merge.has_pending {
		merge.pending_bits = bits
		merge.pending_source_id = source_id
		merge.pending_deadline_ms = now_ms + u64(merge.window_ms)
		merge.has_pending = true
		return true
	}

	if source_id == merge.pending_source_id {
		append(&merge.queued_after_pending, bits)
		return true
	}

	merged_bits := merge.pending_bits | bits
	stroke_merge_queue_output(merge, merged_bits)
	for queued_bits in merge.queued_after_pending {
		stroke_merge_queue_output(merge, queued_bits)
	}
	stroke_merge_clear_pending_only(merge)
	return true
}

stroke_merge_poll :: proc(merge: ^Stroke_Merge, now_ms: u64) -> bool {
	if merge.has_pending && time_reached(now_ms, merge.pending_deadline_ms) {
		stroke_merge_flush_pending(merge)
	}
	return true
}

stroke_merge_next_output :: proc(merge: ^Stroke_Merge) -> (bits: u64, ok: bool) {
	if len(merge.outputs) == 0 {
		return 0, false
	}

	bits = merge.outputs[0]
	ordered_remove(&merge.outputs, 0)
	return bits, true
}

stroke_merge_has_pending :: proc(merge: ^Stroke_Merge) -> bool {
	return merge.has_pending
}
