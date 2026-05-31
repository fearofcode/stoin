#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


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


def stroke_to_bits(stroke: str) -> int | None:
    if not stroke or "/" in stroke:
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
        return "".join(left_and_vowels) + "-" + "".join(right)
    return "".join(left_and_vowels)


def is_plain_translation(value: str) -> bool:
    return (
        value != ""
        and not value.startswith("=")
        and not any(char in value for char in "{}^")
    )


def generate(input_path: Path) -> dict[str, str]:
    source = json.loads(input_path.read_text(encoding="utf-8"))
    output: dict[str, str] = {}

    for stroke, translation in source.items():
        if not isinstance(stroke, str) or not isinstance(translation, str):
            continue
        if not is_plain_translation(translation):
            continue

        bits = stroke_to_bits(stroke)
        if bits is None:
            continue

        canonical = bits_to_stroke(bits)
        output.setdefault(canonical, translation)

    return dict(sorted(output.items()))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate Stoin's starter dictionary from plain single-stroke Lapwing entries."
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
