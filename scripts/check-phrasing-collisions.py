#!/usr/bin/env python3
"""Conservatively check one-stroke phrasing layouts against a Plover dictionary."""

import argparse
import json
import sys
from collections import defaultdict


KEYS = (
    "#", "S-", "T-", "K-", "P-", "W-", "H-", "R-",
    "A", "O", "*", "E", "U",
    "-F", "-R", "-P", "-B", "-L", "-G", "-T", "-S", "-D", "-Z",
)
BITS = {key: 1 << index for index, key in enumerate(KEYS)}
LEFT = {key: key + "-" for key in "STKPWHR"}
RIGHT = {key: "-" + key for key in "FRPBLGTSDZ"}
DIGITS = {
    "1": "S-", "2": "T-", "3": "P-", "4": "H-", "5": "A",
    "0": "O", "6": "-F", "7": "-P", "8": "-L", "9": "-T",
}
VOWELS = set("AO*EU")


def stroke_bits(stroke):
    if not stroke:
        return 0

    number = "#" in stroke or any(char.isdigit() for char in stroke)
    stroke = stroke.replace("#", "")
    result = BITS["#"] if number else 0
    region = "left"

    for char in stroke:
        if char == "-":
            region = "right"
            continue

        if char in DIGITS:
            key = DIGITS[char]
            if key in ("A", "O"):
                region = "vowel"
            elif key.startswith("-"):
                region = "right"
        elif region == "left" and char in LEFT:
            key = LEFT[char]
        elif char in VOWELS:
            key = char
            region = "vowel"
        elif char in RIGHT:
            key = RIGHT[char]
            region = "right"
        else:
            raise ValueError("invalid steno stroke %r" % stroke)

        bit = BITS[key]
        if result & bit:
            raise ValueError("duplicate key %s in stroke %r" % (key, stroke))
        result |= bit

    if result == 0:
        raise ValueError("empty stroke")
    return result


def canonical_stroke(bits):
    left = "".join(key[0] if key.endswith("-") else key for key in KEYS[:13] if bits & BITS[key])
    right = "".join(key[-1] for key in KEYS[13:] if bits & BITS[key])
    if not right:
        return left

    implicit = left + right
    try:
        if stroke_bits(implicit) == bits:
            return implicit
    except ValueError:
        pass
    return left + "-" + right


def add_assignment(assignments, family, bits, description):
    assignments[bits].append((family, description))


def phrasing_assignments(data):
    assignments = defaultdict(list)

    initial = data.get("initial_verbs") or {}
    initial_tails = initial.get("tails") or []
    all_initial_tail_ids = {tail["id"] for tail in initial_tails}
    for stem in initial.get("stems") or []:
        allowed = set(stem.get("tails", all_initial_tail_ids))
        for form in stem["forms"]:
            for tail in initial_tails:
                if tail["id"] not in allowed:
                    continue
                if "stems" in tail and not any(
                    stroke_bits(candidate) == stroke_bits(stem["stroke"])
                    for candidate in tail["stems"]
                ):
                    continue
                if "forms" in tail and not any(
                    stroke_bits(candidate) == stroke_bits(form["stroke"])
                    for candidate in tail["forms"]
                ):
                    continue
                bits = stroke_bits(stem["stroke"]) | stroke_bits(form["stroke"]) | stroke_bits(tail["stroke"])
                add_assignment(
                    assignments,
                    "IV",
                    bits,
                    "%s + %s + %s" % (stem["stroke"], form["stroke"] or "<empty>", tail["stroke"]),
                )

    nonverbs = data.get("nonverbs") or {}
    nonverb_tails = {tail["id"]: tail for tail in nonverbs.get("tails") or []}
    for prefix in nonverbs.get("prefixes") or []:
        for tail_id in prefix["tails"]:
            tail = nonverb_tails[tail_id]
            bits = stroke_bits(prefix["stroke"]) | stroke_bits(tail["stroke"])
            add_assignment(assignments, "NV", bits, "%s + %s" % (prefix["stroke"], tail["stroke"]))

    final = data.get("final_verbs") or {}
    enders = final.get("enders") or []
    all_ender_strokes = {ender["stroke"] for ender in enders}
    contraction = stroke_bits(final["contraction_stroke"]) if final else 0
    for starter in final.get("starters") or []:
        allowed = set(starter.get("enders", all_ender_strokes))
        for operator in final.get("operators") or []:
            for structure in final.get("structures") or []:
                for ender in enders:
                    if ender["stroke"] not in allowed:
                        continue
                    bits = (
                        stroke_bits(starter["stroke"])
                        | stroke_bits(operator["stroke"])
                        | stroke_bits(structure["stroke"])
                        | stroke_bits(ender["stroke"])
                    )
                    description = "%s + %s + %s + %s" % (
                        starter["stroke"],
                        operator["stroke"] or "<empty>",
                        structure["stroke"] or "<empty>",
                        ender["stroke"] or "<empty>",
                    )
                    add_assignment(assignments, "FV", bits, description)
                    add_assignment(assignments, "FV", bits | contraction, "contracted " + description)

    return assignments


def dictionary_assignments(data):
    assignments = defaultdict(list)
    for outline, translation in data.items():
        if "/" in outline:
            continue
        assignments[stroke_bits(outline)].append((outline, translation))
    return assignments


def plain_multiword_translation(translation):
    return (
        isinstance(translation, str)
        and "{" not in translation
        and "}" not in translation
        and len(translation.split()) > 1
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phrasing", help="phrasing JSON path")
    parser.add_argument("dictionary", help="Plover JSON dictionary path")
    parser.add_argument(
        "--show-soft",
        action="store_true",
        help="list allowed collisions with plain multiword dictionary translations",
    )
    args = parser.parse_args()

    with open(args.phrasing, encoding="utf-8") as file:
        phrases = phrasing_assignments(json.load(file))
    with open(args.dictionary, encoding="utf-8") as file:
        dictionary = dictionary_assignments(json.load(file))

    internal = {bits: rows for bits, rows in phrases.items() if len(rows) > 1}
    hard_collisions = {}
    soft_collisions = {}
    for bits, rows in phrases.items():
        if bits not in dictionary:
            continue
        target = soft_collisions if all(
            plain_multiword_translation(translation)
            for _, translation in dictionary[bits]
        ) else hard_collisions
        target[bits] = rows

    for bits, rows in sorted(internal.items()):
        print("internal collision %s:" % canonical_stroke(bits), file=sys.stderr)
        for family, description in rows:
            print("  %s %s" % (family, description), file=sys.stderr)
    for bits, rows in sorted(hard_collisions.items()):
        print("dictionary collision %s:" % canonical_stroke(bits), file=sys.stderr)
        for family, description in rows:
            print("  %s %s" % (family, description), file=sys.stderr)
        for outline, translation in dictionary[bits]:
            print("  dictionary %s -> %s" % (outline, translation), file=sys.stderr)
    if args.show_soft:
        for bits, rows in sorted(soft_collisions.items()):
            print("soft dictionary phrase collision %s:" % canonical_stroke(bits), file=sys.stderr)
            for family, description in rows:
                print("  %s %s" % (family, description), file=sys.stderr)
            for outline, translation in dictionary[bits]:
                print("  dictionary %s -> %s" % (outline, translation), file=sys.stderr)

    family_counts = defaultdict(int)
    for rows in phrases.values():
        family_counts[rows[0][0]] += 1
    print(
        "checked %d phrase outlines (IV %d, FV %d, NV %d) against %d single-stroke dictionary outlines"
        % (
            len(phrases),
            family_counts["IV"],
            family_counts["FV"],
            family_counts["NV"],
            len(dictionary),
        )
    )
    if internal or hard_collisions:
        print(
            "found %d internal, %d hard dictionary, and %d soft dictionary phrase collisions"
            % (len(internal), len(hard_collisions), len(soft_collisions)),
            file=sys.stderr,
        )
        return 1
    print(
        "no hard collisions (%d soft dictionary phrase collisions allowed)"
        % len(soft_collisions)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
