# Brevity Suggestions

Brevity suggestions are an opt-in core-app feature for noticing when the user
types a phrase word-by-word even though the active dictionary has a shorter
outline for the same emitted text.

Enable suggestions with:

```text
--print-suggestions
```

Default: disabled. To collect suggestions without printing them during normal
steno output, pass an append-only JSON Lines log path:

```text
--suggestion-log stoin-suggestions.jsonl
```

Example behavior:

```text
Suggestion: Use TPH-T for "in the"
Suggestion: Use <outline> for "in the beginning"
```

Each log entry includes:

- `unix_time`
- `suggested_outline`
- `typed_outline`
- `text`
- `typed_strokes`
- `suggested_strokes`
- `saved_strokes`

Summarize one or more logs with:

```text
scripts/stoin-suggestion-summary.py stoin-suggestions.jsonl
```

The summary is sorted by descending suggestion frequency, then descending total
saved strokes.

Implementation notes:

- The engine checks the last 2 to 5 visible translations after each successful
  non-command dictionary translation.
- Candidate text uses the actual emitted text, with leading spacing trimmed so
  mid-sentence phrases still match dictionary text like `in the`.
- A suggestion must be shorter than the outline sequence the user just typed.
- Prefer longer text suggestions over shorter overlapping ones when both match.
- Do not suggest the exact outline sequence the user just typed.
- Candidate text and outline buffers are capped at 1024 bytes; oversized
  candidates are skipped.
