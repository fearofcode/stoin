#include "stroke_merge.h"

#include <stddef.h>
#include <string.h>

#include "../third_party/stb_ds.h"

static bool time_reached(uint64_t now_ms, uint64_t deadline_ms)
{
    return (int64_t)(now_ms - deadline_ms) >= 0;
}

static void queue_output(Stroke_Merge *merge, uint64_t bits)
{
    if (merge == NULL || bits == 0) {
        return;
    }
    arrput(merge->outputs, bits);
}

static void clear_pending_only(Stroke_Merge *merge)
{
    if (merge == NULL) {
        return;
    }
    merge->pending_bits = 0;
    merge->pending_deadline_ms = 0;
    merge->pending_source_id = 0;
    merge->has_pending = false;
    arrsetlen(merge->queued_after_pending, 0);
}

static void flush_pending(Stroke_Merge *merge)
{
    if (merge == NULL || !merge->has_pending) {
        return;
    }

    queue_output(merge, merge->pending_bits);
    for (size_t i = 0; i < arrlenu(merge->queued_after_pending); ++i) {
        queue_output(merge, merge->queued_after_pending[i]);
    }
    clear_pending_only(merge);
}

void stroke_merge_init(Stroke_Merge *merge, unsigned int window_ms)
{
    if (merge == NULL) {
        return;
    }
    memset(merge, 0, sizeof(*merge));
    merge->window_ms = window_ms;
}

void stroke_merge_destroy(Stroke_Merge *merge)
{
    if (merge == NULL) {
        return;
    }
    arrfree(merge->queued_after_pending);
    arrfree(merge->outputs);
    memset(merge, 0, sizeof(*merge));
}

void stroke_merge_clear(Stroke_Merge *merge)
{
    if (merge == NULL) {
        return;
    }
    clear_pending_only(merge);
    arrsetlen(merge->outputs, 0);
}

void stroke_merge_set_window_ms(Stroke_Merge *merge, unsigned int window_ms)
{
    if (merge == NULL) {
        return;
    }
    merge->window_ms = window_ms;
    if (window_ms == 0) {
        flush_pending(merge);
    }
}

bool stroke_merge_push(Stroke_Merge *merge, int source_id, uint64_t bits, uint64_t now_ms)
{
    if (merge == NULL) {
        return false;
    }
    if (bits == 0) {
        return true;
    }

    (void)stroke_merge_poll(merge, now_ms);
    if (merge->window_ms == 0) {
        queue_output(merge, bits);
        return true;
    }

    if (!merge->has_pending) {
        merge->pending_bits = bits;
        merge->pending_source_id = source_id;
        merge->pending_deadline_ms = now_ms + merge->window_ms;
        merge->has_pending = true;
        return true;
    }

    if (source_id == merge->pending_source_id) {
        arrput(merge->queued_after_pending, bits);
        return true;
    }

    const uint64_t merged_bits = merge->pending_bits | bits;
    queue_output(merge, merged_bits);
    for (size_t i = 0; i < arrlenu(merge->queued_after_pending); ++i) {
        queue_output(merge, merge->queued_after_pending[i]);
    }
    clear_pending_only(merge);
    return true;
}

bool stroke_merge_poll(Stroke_Merge *merge, uint64_t now_ms)
{
    if (merge == NULL) {
        return false;
    }
    if (merge->has_pending && time_reached(now_ms, merge->pending_deadline_ms)) {
        flush_pending(merge);
    }
    return true;
}

bool stroke_merge_next_output(Stroke_Merge *merge, uint64_t *out_bits)
{
    if (merge == NULL || out_bits == NULL || arrlenu(merge->outputs) == 0) {
        return false;
    }

    *out_bits = merge->outputs[0];
    arrdel(merge->outputs, 0);
    return true;
}

bool stroke_merge_has_pending(const Stroke_Merge *merge)
{
    return merge != NULL && merge->has_pending;
}
