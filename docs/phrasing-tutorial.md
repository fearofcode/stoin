# Core Phrasing Tutorial

This is the practical tutorial for the phrasing code that exists today. For
the broader design sketch, see `docs/phrasing-mode-design.md`; for the current
three-pedal proposal, see `docs/phrasing-three-pedal-reference.md`.

## Current Status

Implemented:

- Core phrase namespace: `PHRASE_NAMESPACE_CORE`.
- Initial-verb `be` phrases only.
- `PW` emits `is`; adding right-hand `D` emits `was`.
- The right hand selects a mnemonic tail such as `the`, `a`, `it`, or `you`.
- USB pedal registration for the core phrase namespace on macOS, Windows, and
  Linux.
- Phrase output goes through normal Stoin translation history, spacing, undo,
  and tracing.
- The `/phrasing` web trainer drills these implemented phrases.

Not implemented yet:

- Other initial verbs.
- Final-verb phrases.
- Non-verb phrase pedal.
- Both-pedal modifier/operator namespace.

Holding the core phrase pedal during a stroke routes that outline through the
phrase engine. For serial machines, tapping the pedal before the stroke also
works: Stoin latches the phrase namespace for the next completed machine
stroke, then clears it.

## Register The Core Pedal

On macOS or Linux:

```sh
make
./build/macos/stoin --register-pedal core
```

Use `./build/linux/stoin --register-pedal core` on Linux.

When prompted, press the pedal you want to use for core phrase mode. Stoin saves
the mapping in local `stoin-pedals.json`, which is ignored by git. After
registration, the app continues normally. Future runs can just use:

```sh
./build/macos/stoin
```

For qwerty testing:

```sh
./build/macos/stoin --input qwerty
```

Without a pedal, tap Shift by itself to route the next qwerty steno chord
through core phrase mode. For example, tap Shift, release it, then chord
`PW-PB` to emit `is a`. If Shift is chorded with other qwerty steno keys, it
stays part of the steno chord instead of arming phrase mode.

If your pedal acts like a keyboard key, map it to F13-F24 before registering
it. Do not map it to `a`, another ordinary letter, space, enter, or
punctuation. Stoin ignores registered text-producing keyboard keys because
macOS may still type them into the active app, and a downstream event tap cannot
reliably tell the pedal's `a` apart from the `a` on your real keyboard.

## Run The Current Smoke Test

```sh
make test
go test ./cmd/stoin-srs-web
```

The relevant C tests are in `tests/test_steno.c`; search for `core phrase`.

## Paper Tape

When stroke tracing is enabled, phrase-mode rows are marked on the left side of
the tape:

```text
PW-PB [phrase] -> is a
#KW [phrase fallback] -> test
SAO [phrase fallback] -> [untranslated]
```

`[phrase]` means the phrase engine generated the output. `[phrase fallback]`
means a phrase pedal was active, but the phrase engine missed and Stoin used
the regular dictionary/raw-steno path.

## Core Phrase Shape

A core phrase stroke is currently interpreted as:

```text
PW + optional right D + optional right-hand tail
```

Examples:

```text
PW       is
PW-D     was
PW-PB    is a
PW-PBD   was a
PW-T     is the
PW-TD    was the
```

`D` is reserved for past tense in this bank, so it is not part of any tail
assignment.

## Implemented Tail Bank

```text
empty   no tail
T       the
PB      a/an
P       it
RT      that
TS      this
SZ      these
TZ      those
PL      me
RP      you
R       your
S       us
FR      her
FL      him
PLT     them
L       all
PBT     one
```

## First Practice Set

When the core phrase pedal exists, hold it and stroke:

```text
PW       is
PW-D     was
PW-T     is the
PW-TD    was the
PW-PB    is a
PW-PBD   was a
PW-P     is it
PW-PD    was it
PW-RT    is that
PW-RTD   was that
PW-TS    is this
PW-TSD   was this
PW-SZ    is these
PW-TZ    is those
PW-PL    is me
PW-RP    is you
PW-R     is your
PW-S     is us
PW-FR    is her
PW-FL    is him
PW-PLT   is them
PW-L     is all
PW-PBT   is one
```

Without the core phrase pedal, these are ordinary steno strokes. For example,
`PW-PB` currently stays in normal dictionary/raw-steno mode and emits `PW-PB`
if it is untranslated. With the pedal held, a stroke that is not part of the
phrase grammar falls back to the regular dictionary stack, so you can keep the
pedal down while writing an ordinary word between phrases.

## QWERTY Layout Hints

These are based on the current `stoin.keymap`, not `tests/test.keymap`.

```text
left P   d
left W   c
right F  j
right R  m
right P  k
right B  comma
right L  l
right T  semicolon
right S  slash
right D  apostrophe
right Z  Right Shift
```

Example qwerty chords:

```text
PW-PB    d + c + k + comma
PW-PBD   d + c + k + comma + apostrophe
PW-T     d + c + semicolon
PW-RT    d + c + m + semicolon
PW-TS    d + c + semicolon + slash
PW-TZ    d + c + semicolon + Right Shift
PW-RP    d + c + m + k
```

## Web Trainer

Run the trainer:

```sh
make srs-web
```

Then open:

```text
http://127.0.0.1:8080/phrasing
```

The trainer shows the target phrase, optionally shows the phrase-mode outline
as a hint, and accepts the text produced by your steno output. Current-lesson
mode walks one progression step at a time; cumulative-random mode draws from
every lesson up to the selected one.
