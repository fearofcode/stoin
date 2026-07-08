#!/usr/bin/env python3
"""Summarize stoin brevity suggestion JSONL logs."""

import argparse
import collections
import json
import sys


def iter_entries(paths):
    for path in paths:
        if path == "-":
            yield from iter_file(sys.stdin, "<stdin>")
            continue
        with open(path, "r", encoding="utf-8") as file:
            yield from iter_file(file, path)


def iter_file(file, label):
    for line_number, line in enumerate(file, start=1):
        line = line.strip()
        if not line:
            continue
        try:
            entry = json.loads(line)
        except json.JSONDecodeError as error:
            print(
                f"{label}:{line_number}: skipping invalid JSON: {error}",
                file=sys.stderr,
            )
            continue
        if not isinstance(entry, dict):
            print(f"{label}:{line_number}: skipping non-object entry", file=sys.stderr)
            continue
        yield label, line_number, entry


def int_field(entry, field):
    value = entry.get(field, 0)
    if isinstance(value, bool):
        return 0
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    return 0


def summarize(paths):
    summaries = {}
    for label, line_number, entry in iter_entries(paths):
        outline = entry.get("suggested_outline")
        text = entry.get("text")
        if not isinstance(outline, str) or not isinstance(text, str):
            print(
                f"{label}:{line_number}: skipping entry without suggested_outline/text",
                file=sys.stderr,
            )
            continue

        key = (outline, text)
        summary = summaries.setdefault(
            key,
            {
                "count": 0,
                "saved_strokes": 0,
                "typed_outlines": collections.Counter(),
            },
        )
        summary["count"] += 1
        summary["saved_strokes"] += int_field(entry, "saved_strokes")

        typed_outline = entry.get("typed_outline")
        if isinstance(typed_outline, str) and typed_outline:
            summary["typed_outlines"][typed_outline] += 1

    return summaries


def format_typed_outlines(counter, limit):
    if not counter:
        return ""
    parts = []
    for outline, count in counter.most_common(limit):
        parts.append(f"{outline} x{count}")
    remaining = len(counter) - len(parts)
    if remaining > 0:
        parts.append(f"+{remaining} more")
    return ", ".join(parts)


def print_summary(summaries, limit, typed_limit):
    rows = []
    for (outline, text), summary in summaries.items():
        count = summary["count"]
        saved = summary["saved_strokes"]
        rows.append(
            {
                "count": count,
                "saved": saved,
                "avg_saved": saved / count if count else 0.0,
                "outline": outline,
                "text": text,
                "typed": format_typed_outlines(summary["typed_outlines"], typed_limit),
            }
        )

    rows.sort(key=lambda row: (-row["count"], -row["saved"], row["outline"], row["text"]))
    if limit is not None:
        rows = rows[:limit]

    print("count\tsaved\tavg_saved\toutline\ttext\ttyped_outlines")
    for row in rows:
        print(
            "\t".join(
                [
                    str(row["count"]),
                    str(row["saved"]),
                    f"{row['avg_saved']:.2f}",
                    row["outline"],
                    json.dumps(row["text"], ensure_ascii=False),
                    row["typed"],
                ]
            )
        )


def main():
    parser = argparse.ArgumentParser(
        description="Print stoin brevity suggestions grouped by frequency."
    )
    parser.add_argument(
        "logs",
        nargs="+",
        help="JSONL suggestion log path(s), or '-' for stdin.",
    )
    parser.add_argument(
        "-n",
        "--limit",
        type=int,
        default=None,
        help="maximum number of rows to print",
    )
    parser.add_argument(
        "--typed-limit",
        type=int,
        default=3,
        help="number of typed outline variants to show per suggestion",
    )
    args = parser.parse_args()

    summaries = summarize(args.logs)
    print_summary(summaries, args.limit, args.typed_limit)


if __name__ == "__main__":
    main()
