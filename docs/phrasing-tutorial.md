# Keyboard-Only Phrasing Tutorial

Phrasing is now ordinary steno strokes only. There is no external activation,
no alternate stroke mode, and no follow-on modifier strokes.

The phrase matcher runs before loaded dictionaries. If a stroke is assigned in
the phrasing reference, the phrase wins. If a stroke is not assigned, Stoin uses
the normal dictionary/raw-steno path.

Use `docs/phrasing-keyboard-only-design.md` as the reference table for every
implemented assignment.

## Practice Path

1. Learn `IV Set 1`: `PW-B`, `PW-T`, `PW-P`, and `PW-RT`, plus right `D` for
   `was`.
2. Learn the `FV` starter table, then practice common long forms such as
   `SKWHR-B`, `SKWHR-PBG`, `SKWHRAO-G`, `SKWHREG`, and `SKWHR-FG`.
3. Add `#` only for contraction forms, such as `#SKWHR-B`, `#SKWHR*E`, and
   `#SKWHRAO-G`.
4. Learn the immediate `NV` rows first: `WHR*-T`, `WHR*-PLT`, `WHR*-RT`,
   `PHR*-RT`, and `KPHR*-RT`.

## Paper Tape

Assigned phrase strokes are marked as phrase hits:

```text
PWB [phrase] -> is a
SKWHRB [phrase] -> she is
WHR*RT [phrase] -> with that
```

Unassigned strokes are ordinary dictionary/raw strokes and have no phrase
fallback label.

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
