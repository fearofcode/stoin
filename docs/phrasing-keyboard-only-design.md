# Non-Pedal Phrasing Reference

Phrases are ordinary one-stroke chords. They do not use a pedal, phrase mode,
or separate runtime namespace.

The main dictionary stack is checked first. If it has no translation for the
stroke, Stoin checks the generated IV, NV, and FV phrase banks. This makes an
explicit dictionary entry an override while allowing a generated phrase to be
retroactively replaced when a later stroke completes a longer dictionary
outline.

## Collision rule

Every structurally possible phrase outline must be absent from Phoenix, not
just the outlines shown in examples. Phrase families must also be disjoint from
one another.

Run the conservative structural audit with:

```sh
scripts/check-phrasing-collisions.py phrasing.json ~/Downloads/phoenix.json
```

The checker understands number-bar aliases which appear in Phoenix, ignores
multi-stroke dictionary entries because every generated phrase is one stroke,
and fails on either an internal phrase collision or a Phoenix collision. It
checks the full FV Cartesian product, including combinations the formatter
later rejects, so a clean result is stronger than checking only phrases which
currently emit text.

## Family summary

| Family | Layout | Purpose |
| --- | --- | --- |
| IV | `unique O-bearing starter + form + tail` | verb-first phrases such as `say it` |
| FV | `unique subject starter + operator + structure + ender` | subject-first phrases such as `she has gone` |
| NV | `unique left starter + tail` | nonverb phrases such as `with a` |

None of the generated phrase outlines uses the number bar. IV reserves ordinary
thumb `O` inside the chord, while NV uses two- and three-key left-hand starters.
FV uses Phoenix-empty left-hand starter banks, and contracted FV outlines add
ordinary thumb `U`. These are normal dictionary outlines, not mode switches or
special runtime keys.

## Initial-verb phrases

IV outlines are constructed as:

```text
O-bearing starter + form + right-hand tail
```

### IV stems

The table shows the complete starter component. Every IV starter includes `O`,
which separates IV from the NV family without adding another stroke.

| Stem | Verb | Stem | Verb |
| --- | --- | --- | --- |
| `SKPO` | be | `TKPO` | have |
| `STWO` | see | `TPWO` | say |
| `STHO` | think | `SKHO` | tell |
| `TKHO` | catch | `KPHO` | look |
| `SWHO` | hold | `TWHO` | sell |
| `KWHO` | spell | `PWHO` | pull |
| `KPRO` | put | `WHRO` | keep |
| `STKWO` | call | `STPWO` | do |
| `SPRO` | go | `SKPWO` | want |
| `STKHO` | ask | `WHO` | happen |
| `SKPHO` | feel | `TWRO` | come |
| `TKPHO` | know | `SKWHO` | get |
| `SPWHO` | believe | `TPWHO` | become |
| `STKRO` | run | `STPRO` | make |
| `SKPRO` | take | `TKPRO` | find |
| `TKWRO` | give | `SPWRO` | use |
| `TPWRO` | work | `KPWRO` | need |
| `STHRO` | remember | `SKHRO` | understand |
| `KPHRO` | try | `TWHRO` | expect |

### IV forms

| Form component | Meaning |
| --- | --- |
| empty | third-person present |
| `-D` | simple past |
| `E` | base/non-third present |
| `E-D` | plural past for `be` |
| `*` | present participle/gerund |
| `U` | infinitive with `to` |
| `EU` | bare `be` form |
| `A` | `can` + base verb |
| `A-D` | `could` + base verb |

The star is reserved for the IV progressive form. `O` is reserved by every IV
starter and therefore cannot collapse into a form component.

### IV tails

| Tail | Text | Tail | Text |
| --- | --- | --- | --- |
| `-B` | a | `-PB` | an |
| `-BL` | like | `-T` | the |
| `-LT` | at | `-RP` | up |
| `-SZ` | out | `-RB` | with |
| `-P` | it | `-S` | us |
| `-R` | her | `-Z` | his |
| `-RZ` | your | `-FB` | of |
| `-PL` | my | `-PLS` | myself |
| `-PLT` | me | `-RT` | that |

Per-verb allowlists in `phrasing.json` remove ungrammatical or low-value
combinations. The selected starters also keep the allowlisted universe clean;
for example, adding all forms of `go the` and `come the` would create the
Phoenix outlines `SPROUT` (sprout) and `TWROET` (typewrote).

Examples:

| Outline | Output |
| --- | --- |
| `SKPO-B` | is a |
| `SKPAO-BD` | could be a |
| `TPWO-P` | says it |
| `TPWO*-P` | saying it |
| `STHOU-RT` | to think that |
| `WHROU-P` | to keep it |

## Final-verb phrases

FV outlines retain the existing grammar:

```text
starter + operator + structure + ender
```

The starter assignments follow Jeff phrasing's unique-starter idea, but the
seven starters which occupy Phoenix left-hand banks gain one extra left-hand
key. Every selected bank is completely empty in Phoenix.

### FV starters

| Starter | Subject / agreement |
| --- | --- |
| `SWHR` | I / first singular |
| `SKPWR` | you / plural agreement |
| `KWHR` | he / third singular |
| `SKWHR` | she / third singular |
| `KPWH` | it / third singular |
| `STWR` | we / plural agreement |
| `TKWH` | they / plural agreement |
| `STKWH` | this / third singular |
| `STWH` | that / third singular |
| `STWHR` | there / third singular |
| `STPWHR` | there / plural agreement |

### FV operators and structures

| Operator | Meaning |
| --- | --- |
| empty / `*` | ordinary / negative |
| `A` / `A*` | can / cannot |
| `O` / `O*` | should / should not |
| `AO` / `AO*` | will / will not |

| Structure | Meaning |
| --- | --- |
| empty | simple |
| `E` | progressive |
| `-F` | perfect |
| `E-F` | perfect progressive |

Enders and their optional continuation words are defined in `phrasing.json`.
No ender contains `-F`, which is reserved for the perfect structure. The hold
ender is therefore `-PBL` / `-PBLD` rather than the old `-FPL` pair.

Adding `U` requests the grammatically valid contracted version of an FV
outline. Long form remains the default.

Examples:

| Outline | Output |
| --- | --- |
| `SKWHR-B` | she is |
| `SKWHRE-G` | she is going |
| `SWHR-FPBG` | I have thought |
| `TKWHAO-RLT` | they will tell |
| `SKWHR*U-RLT` | she doesn't tell |
| `STWHAOU` | that'll |

## Nonverb phrases

NV prefixes are unique two- and three-key left-hand starters. They do not need
a family marker because no selected NV outline overlaps IV, FV, or Phoenix.

| Outline prefix | Text | Outline prefix | Text |
| --- | --- | --- | --- |
| `TW` | with | `STP` | at |
| `SKP` | it | `SKH` | off |
| `TR` | up | `STW` | can |
| `TPW` | just | `STH` | all |
| `SKW` | if | `TKP` | only |
| `TKH` | but | `SWH` | that |
| `TWH` | for | `KWH` | of |
| `PWH` | anything | `TWR` | as |
| `WHR` | even |  |  |

NV tails are defined in `phrasing.json`. They include `a`, `an`, `like`,
`the`, pronouns, `that`, `can`, `if`, `though`, and `else`, with per-prefix
allowlists. The `can` tail is `-G`; `-BG` would make `as can` collide with
Phoenix's `10:00` outline.

Examples:

| Outline | Output |
| --- | --- |
| `TW-B` | with a |
| `PWH-BL` | anything like |
| `TWR-F` | as if |
| `WHR-GT` | even though |
