#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Optional


STENO_ORDER = [
    "#",
    "S",
    "T",
    "K",
    "P",
    "W",
    "H",
    "R",
    "A",
    "O",
    "*",
    "E",
    "U",
    "-F",
    "-R",
    "-P",
    "-B",
    "-L",
    "-G",
    "-T",
    "-S",
    "-D",
    "-Z",
]

LEFT = {
    "#": 0,
    "S": 1,
    "T": 2,
    "K": 3,
    "P": 4,
    "W": 5,
    "H": 6,
    "R": 7,
}
VOWELS = {
    "A": 8,
    "O": 9,
    "*": 10,
    "E": 11,
    "U": 12,
}
RIGHT = {
    "F": 13,
    "R": 14,
    "P": 15,
    "B": 16,
    "L": 17,
    "G": 18,
    "T": 19,
    "S": 20,
    "D": 21,
    "Z": 22,
}
RIGHT_BITS = {1 << index for index in RIGHT.values()}


def stroke_to_bits(stroke: str) -> Optional[int]:
    if not stroke:
        return None

    bits = 0
    region = "left"

    for char in stroke:
        if char == "-":
            region = "right"
            continue

        index = None
        if region == "left":
            index = LEFT.get(char)
            if index is None:
                index = VOWELS.get(char)
                if index is not None:
                    region = "vowel"
            if index is None:
                index = RIGHT.get(char)
                if index is not None:
                    region = "right"
        elif region == "vowel":
            index = VOWELS.get(char)
            if index is None:
                index = RIGHT.get(char)
                if index is not None:
                    region = "right"
        else:
            index = RIGHT.get(char)

        if index is None:
            return None

        bit = 1 << index
        if bits & bit:
            return None
        bits |= bit

    return bits or None


def bits_to_stroke(bits: int) -> str:
    left_and_vowels = []
    right = []

    for index, key in enumerate(STENO_ORDER):
        if not bits & (1 << index):
            continue
        if key.startswith("-"):
            right.append(key[1:])
        else:
            left_and_vowels.append(key)

    if right:
        implicit = "".join(left_and_vowels + right)
        if stroke_to_bits(implicit) == bits:
            return implicit
        return "".join(left_and_vowels) + "-" + "".join(right)
    return "".join(left_and_vowels)


def outline_to_canonical(outline: str) -> Optional[str]:
    strokes = outline.split("/")
    if not strokes:
        return None

    canonical = []
    for stroke in strokes:
        bits = stroke_to_bits(stroke)
        if bits is None:
            return None
        canonical.append(bits_to_stroke(bits))

    return "/".join(canonical)


def is_supported_meta(meta: str) -> bool:
    if meta.startswith("&"):
        return True
    if meta.startswith(":glue:") or meta.startswith("glue:"):
        return True
    if meta.startswith(":stitch:"):
        return True
    if meta == ":stitch_last_word" or meta.startswith(":stitch_last_word:"):
        return True
    if meta == ":stitch_phrase" or meta.startswith(":stitch_phrase:"):
        return True
    if meta == ":attach" or meta.startswith(":attach:"):
        return True
    if "~" in meta or "|" in meta:
        return False
    return meta.startswith("^") or meta.endswith("^")


def is_supported_translation(value: str) -> bool:
    if value == "=undo":
        return True
    if value == "" or value.startswith("="):
        return False

    index = 0
    while index < len(value):
        char = value[index]
        if char == "^" or char == "}":
            return False
        if char == "\\":
            index += 2
            continue
        if char != "{":
            index += 1
            continue

        end = index + 1
        while end < len(value):
            if value[end] == "\\":
                end += 2
                continue
            if value[end] == "}":
                break
            end += 1
        if end >= len(value) or not is_supported_meta(value[index + 1:end]):
            return False
        index = end + 1

    return True


def generate(input_path: Path) -> dict[str, str]:
    source = json.loads(input_path.read_text(encoding="utf-8"))
    output: dict[str, str] = {}

    for stroke, translation in source.items():
        if not isinstance(stroke, str) or not isinstance(translation, str):
            continue
        if not is_supported_translation(translation):
            continue

        canonical = outline_to_canonical(stroke)
        if canonical is None:
            continue

        output.setdefault(canonical, translation)

    return dict(sorted(output.items()))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate Stoin's starter dictionary from supported Lapwing entries."
    )
    parser.add_argument("--input", default="lapwing-base.json", type=Path)
    parser.add_argument("--output", default="stoin-dictionary.json", type=Path)
    args = parser.parse_args()

    dictionary = generate(args.input)
    args.output.write_text(
        json.dumps(dictionary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {len(dictionary)} entries to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
