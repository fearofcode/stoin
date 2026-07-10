#ifndef STROKE_MERGE_H
#define STROKE_MERGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Stroke_Merge {
    uint64_t pending_bits;
    uint64_t pending_deadline_ms;
    uint64_t *queued_after_pending;
    uint64_t *outputs;
    int pending_source_id;
    unsigned int window_ms;
    bool has_pending;
} Stroke_Merge;

void stroke_merge_init(Stroke_Merge *merge, unsigned int window_ms);
void stroke_merge_destroy(Stroke_Merge *merge);
void stroke_merge_clear(Stroke_Merge *merge);
void stroke_merge_set_window_ms(Stroke_Merge *merge, unsigned int window_ms);
bool stroke_merge_push(Stroke_Merge *merge, int source_id, uint64_t bits, uint64_t now_ms);
bool stroke_merge_poll(Stroke_Merge *merge, uint64_t now_ms);
bool stroke_merge_next_output(Stroke_Merge *merge, uint64_t *out_bits);
bool stroke_merge_has_pending(const Stroke_Merge *merge);

#endif
