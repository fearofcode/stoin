# Keyboard-Only Phrasing Tutorial

Phrasing is now ordinary steno strokes only. There is no external activation,
no alternate stroke mode, and no follow-on modifier strokes.

The phrase matcher runs before loaded dictionaries. If a stroke is assigned in
the phrasing reference, the phrase wins. If a stroke is not assigned, Stoin uses
the normal dictionary/raw-steno path.

Use `docs/phrasing-keyboard-only-design.md` as the reference table for every
implemented assignment.

## Practice Path

1. Learn `IV Set 1`: `PW` for `is/was/are/were`, `H` for `has/had/have`,
   `E` for base/non-third forms, and the shared right-hand tails `B`, `T`,
   `P`, and `RT`.
2. Learn the `FV` starter table, then practice common long forms such as
   `SKWHR-B`, `SKWHR-PBG`, `SKWHRAO-G`, `SKWHREG`, and `SKWHR-FG`.
3. Add `#` only for contraction forms, such as `#SKWHR-B`, `#SKWHR*E`, and
   `#SKWHRAO-G`.
4. Learn the immediate `NV` rows first: `WHR*-B`, `WHR*-T`, `WHR*-PLT`,
   `WHR*-RT`, `PHR*-RT`, `KPHR*-B`, and `KPHR*-RT`.

## Paper Tape

Assigned phrase strokes are marked as phrase hits:

```text
PWB [phrase] -> is a
HBD [phrase] -> had a
PWEB [phrase] -> are a
HEB [phrase] -> have a
SKWHRB [phrase] -> she is
WHR*RT [phrase] -> with that
WHR*B [phrase] -> with a
KPHR*B [phrase] -> even a
```

Unassigned strokes are ordinary dictionary/raw strokes and have no phrase
fallback label.

One-off `NV` chunks such as `anything else`, `one of them`, and `instead of`
live in `stoin-custom.json`. The default config already loads it; if you
override dictionaries on the command line, include that file with another
`--dictionary` argument.

## Web Trainer

Run the trainer:

```sh
make srs-web
```

Then open:

```text
http://127.0.0.1:8080/phrasing
```

The trainer drills the implemented `IV`, `FV`, and `NV` assignment sets. The
target phrase is shown as the prompt; the stroke appears as the hint.
