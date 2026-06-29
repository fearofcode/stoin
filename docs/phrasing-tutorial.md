# Core Phrasing Tutorial

This is the practical tutorial for the phrasing code that exists today. For the broader plan, see `docs/phrasing-mode-design.md`.

## Current Status

Implemented:

- Core phrase namespace: `PHRASE_NAMESPACE_CORE`.
- Core phrase grammar for subject, tense, auxiliary, negation, aspect, inversion, verb, and tail.
- Mock-pedal tests using `steno_set_phrase_namespace()`.
- macOS USB HID pedal registration for the core phrase namespace.
- Phrase output goes through normal Stoin translation history, spacing, undo, and tracing.

Not implemented yet:

- Runtime pedal input on Linux.
- Non-verb phrase pedal.
- Both-pedal modifier/operator namespace.
- SRS/practice deck integration.

So today, the phrase engine is reachable on macOS with a registered USB HID pedal. Holding the core phrase pedal during a stroke routes that outline through the phrase engine. For serial machines, tapping the pedal before the stroke also works: Stoin latches the phrase namespace for the next completed machine stroke, then clears it.

## Register The Core Pedal

On macOS:

```sh
make
./build/macos/stoin --register-pedal core
```

When prompted, press the pedal you want to use for core phrase mode. Stoin saves the mapping in local `stoin-pedals.json`, which is ignored by git. After registration, the app continues normally. Future runs can just use:

```sh
./build/macos/stoin
```

For qwerty testing:

```sh
./build/macos/stoin --input qwerty
```

Without a pedal, tap Shift by itself to route the next qwerty steno chord through core phrase mode. For example, tap Shift, release it, then chord `P-BS` to emit `it is a`. If Shift is chorded with other qwerty steno keys, it stays part of the steno chord instead of arming phrase mode.

If your pedal acts like a keyboard key, map it to F13-F24 before registering it. Do not map it to `a`, another ordinary letter, space, enter, or punctuation. Stoin ignores registered text-producing keyboard keys because macOS may still type them into the active app, and a downstream event tap cannot reliably tell the pedal's `a` apart from the `a` on your real keyboard.

## Run The Current Smoke Test

```sh
make test
```

The relevant tests are in `tests/test_steno.c`; search for `core phrase`.

## Paper Tape

When stroke tracing is enabled, phrase-mode rows are marked on the left side of the tape:

```text
PBS [phrase] -> it is a
TEFT [phrase fallback] -> test
SAO [phrase fallback] -> [untranslated]
```

`[phrase]` means the phrase engine generated the output. `[phrase fallback]` means a phrase pedal was active, but the phrase engine missed and Stoin used the regular dictionary/raw-steno path.

## Core Phrase Shape

A core phrase stroke is interpreted as:

```text
starter + grammar/state + verb + tail
```

The fields are:

```text
left S T K P W        starter
left H R + A O * E U  grammar/state
right F R P B L G     verb
right T S D Z         tail
```

## Implemented Starters

```text
empty  bare infinitive / empty starter
S      I
W      we
K      he
SK     she
P      it
T      they
ST     that
PW     you
```

## Implemented Grammar Bits

```text
H      past / conditional
A      can / could
O      should, or with A: will / would
A+O    will / would
*      negative
E      progressive: be + -ing
R      perfect: have + past participle
E+R    perfect progressive
U      inverted/question order
```

Examples:

```text
K-G      he goes
KH-G     he went
K*G      he doesn't go
KA-G     he can go
KHA-G    he could go
KAO-G    he will go
KHAO-G   he would go
KE-G     he is going
KR-G     he has gone
KRE-G    he has been going
KU-G     does he go
KAU-G    can he go
```

## Implemented Verbs

```text
B     be
G     go
BL    believe
RPB   understand
```

These are mnemonic anchors and should be considered stable.

## Implemented Tails

```text
empty   no tail
T       the
S       a
D       it
Z       that
TS      this
TD      me
TZ      those
SD      her
SZ      us
DZ      them
TSD     you
TSZ     these
TDZ     him
SDZ     one
TSDZ    all
```

## First Practice Set

When the core phrase pedal exists, hold it and stroke:

```text
P-BS          it is a
TH*G          they didn't go
-B            to be
S-B           I am
K-G           he goes
TH-B          they were
SKH-BLSD      she believed her
KA-G          he can go
KHAO-G        he would go
KE-G          he is going
KR-G          he has gone
KRE-G         he has been going
KU-G          does he go
K*U-G         doesn't he go
KAU-G         can he go
SKHRAO-BLSD   she would have believed her
K-RPBT        he understands the
```

Without the core phrase pedal, these are ordinary steno strokes. For example, `P-BS` currently stays in normal dictionary/raw-steno mode and emits `PBS` if it is untranslated. With the pedal held, a stroke that is not part of the phrase grammar falls back to the regular dictionary stack, so you can keep the pedal down while writing an ordinary word between phrases.

## QWERTY Layout Hints

These are based on the current `stoin.keymap`, not `tests/test.keymap`.

Left hand:

```text
S z
T s
K x
P d
W c
H f
R v
```

Thumb/star:

```text
A Space
O Backspace
E Tab
U Enter
* g, b, h, or n
```

Right hand:

```text
F j
R m
P k
B ,
L l
G .
T ;
S /
D '
Z Right Shift
```

Example qwerty chords for the first practice set:

```text
P-BS          d + , + /
TH*G          s + f + g + .
-B            ,
S-B           z + ,
K-G           x + .
TH-B          s + f + ,
SKH-BLSD      z + x + f + , + l + / + '
KA-G          x + Space + .
KHAO-G        x + f + Space + Backspace + .
KE-G          x + Tab + .
KR-G          x + v + .
KRE-G         x + v + Tab + .
KU-G          x + Enter + .
K*U-G         x + g + Enter + .
KAU-G         x + Space + Enter + .
SKHRAO-BLSD   z + x + f + v + Space + Backspace + , + l + / + '
K-RPBT        x + m + k + , + ;
```

## What To Look For

The first things to validate once pedal input is wired:

- Holding or tapping the core pedal before the next serial stroke changes `P-BS` from raw `PBS` to `it is a`.
- Releasing the core pedal returns the same stroke to normal dictionary behavior.
- Phrase output gets normal leading spaces between phrases.
- `=undo` can undo a phrase translation.
- Modal inversion works: `KAU-G` should be `can he go`, not `can he goes`.

## SRS Next Step

If these outlines feel stable in practice, the SRS app can start with this table:

```text
P-BS          it is a
TH*G          they didn't go
-B            to be
S-B           I am
K-G           he goes
TH-B          they were
SKH-BLSD      she believed her
KA-G          he can go
KHAO-G        he would go
KE-G          he is going
KR-G          he has gone
KRE-G         he has been going
KU-G          does he go
K*U-G         doesn't he go
KAU-G         can he go
SKHRAO-BLSD   she would have believed her
K-RPBT        he understands the
```

The SRS prompt should show the English phrase and ask for the phrase-mode outline. It should mark the answer as a phrase-mode stroke, because the same outline without the pedal may mean something else or fall through to raw steno.
