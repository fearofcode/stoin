# Stoin Agent Context

This repo is a C stenography app. The code can tell you where things live; this file is for the stenography concepts that are easy to miss if you have not worked on steno software before.

## Steno Mental Model

Steno input is chorded. A stroke is a set of steno keys pressed together, not a sequence of typed letters. The printed outline is a canonical ordering of the keys in that set.

Standard steno order is:

```text
# S T K P W H R A O * E U F R P B L G T S D Z
```

The left-hand consonants are:

```text
S T K P W H R
```

The thumb/vowel bank is:

```text
A O * E U
```

The right-hand consonants are:

```text
F R P B L G T S D Z
```

Some letters exist on both hands, so hand position matters. For example, left `R` and right `R` are different steno keys. In outline notation, `-` is used when needed to disambiguate the right-hand side, so `R-R` means left `R` plus right `R`. Hyphens are a notation device, not a physical key.

Many right-hand keys can be printed without `-` when they are unambiguous. For example, a raw drill chord might prefer `SAP` over `SA-P`, even though the `P` is on the right hand. This is normal steno display convention.

## Number Bar

The number bar is represented by `#`. On many machines the number positions are really `#` plus ordinary steno keys. Dictionary outlines may use either digit notation or the underlying key notation.

Number-bar digit aliases:

```text
1 = #S
2 = #T
3 = #P
4 = #H
5 = #A
0 = #O
6 = #-F
7 = #-P
8 = #-L
9 = #-T
```

So an outline such as `#*-678` is equivalent to `#*FPL` internally. It is okay if tracing or dictionary dumps show the canonical steno-key spelling rather than preserving the original digit spelling.

## Outlines And Dictionaries

A dictionary key is an outline. Multi-stroke outlines use `/`, for example:

```text
STOER/-Z
#*-6R/TKPWRA-PBD
```

Translation should generally prefer the longest suffix of the stroke history that matches a dictionary entry. If a later stroke turns a previous shorter match into a longer match, the app may need to retroactively delete and replace emitted text.

Untranslated strokes are useful for drills and should emit raw steno when no dictionary entry matches.

Dictionary entries may be ordinary text or Plover-style translation commands. Existing Plover/Lapwing dictionary compatibility is an important design goal, so prefer accepting normal Plover outline spellings rather than inventing project-specific syntax.

## Hardware Versus Steno Layout

Do not confuse QWERTY key names with steno keys. The qwerty mode maps keyboard keys to steno bits; TX Bolt and Gemini PR devices send steno strokes directly at the protocol level.

The same steno stroke should behave the same way regardless of whether it came from qwerty chord gathering or a real steno machine.

## Practical Notes

When touching steno parsing, serial decoding, translation matching, retroactive behavior, or command handling, add tests. Steno edge cases are compact and easy to regress.

The project intentionally uses bitsets for stroke identity. Treat outline strings as user-facing notation for those bits.
