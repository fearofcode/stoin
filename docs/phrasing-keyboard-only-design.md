# Non-Pedal Phrasing Reference

Phrases are ordinary one-stroke chords. They do not use a pedal, phrase mode,
or separate runtime namespace.

Stoin checks generated IV, NV, and FV phrases alongside the main dictionary
stack. Single-token dictionary words and Plover command translations win.
Generated phrasing wins when the competing one-stroke dictionary translation
is plain multiword text. A later stroke can still retroactively replace either
result when it completes a longer dictionary outline.

## Collision rule

Phrase families must be disjoint from one another. Every structurally possible
phrase outline must also avoid hard Phoenix collisions: single-token
translations and translations containing Plover commands. A Phoenix value is
soft when it contains at least two whitespace-separated tokens and no `{` or
`}` command syntax; generated phrasing intentionally takes precedence for
those outlines.

Run the conservative structural audit with:

```sh
scripts/check-phrasing-collisions.py phrasing.json ~/Downloads/phoenix.json
```

Add `--show-soft` to list the allowed Phoenix phrase collisions. The checker
understands number-bar aliases, ignores multi-stroke dictionary entries because
every generated phrase is one stroke, and checks the full FV Cartesian product.
It fails on internal collisions and hard Phoenix collisions. Phoenix is the
only compatibility target for the checked-in layout.

The checked-in layout has 19 soft Phoenix collisions and no hard or internal
collisions. Some produce the same text (`SKP*F` is `and of` in both systems);
others intentionally replace a Phoenix phrase, such as `SKP*G` changing from
`and go` to generated `and can`.

## Family summary

| Family | Layout | Purpose |
| --- | --- | --- |
| IV | `unique O-bearing starter + form + tail` | verb-first phrases such as `say it` |
| FV | `unique subject starter + operator + structure + ender` | subject-first phrases such as `she has gone` |
| NV | `mnemonic prefix + shared tail` | nonverb phrases such as `with a` |

None of the generated phrase outlines uses the number bar. IV reserves ordinary
thumb `O` inside the chord. NV prefixes may use left-hand, vowel, star, and
right-hand mnemonic keys. FV uses Phoenix-empty left-hand starter banks, and
contracted FV outlines add ordinary thumb `U`. These are normal dictionary
outlines, not mode switches or special runtime keys.

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
| `SKPWO` | be | `TKPO` | have |
| `SPWHO` | see | `STHO` | say |
| `TWHO` | think | `TWHRO` | tell |
| `KPHO` | catch | `WHRO` | look |
| `TKHO` | hold | `STHRO` | sell |
| `SPWRO` | spell | `KPWRO` | pull |
| `STPRO` | put | `KPRO` | keep |
| `SKPHO` | call | `STKHO` | do |
| `KPWHO` | go | `SKWHO` | want |
| `SKPRO` | ask | `STPO` | happen |
| `SWHO` | feel | `TKHRO` | come |
| `TPWHO` | know | `TKPHO` | get |
| `TPWRO` | believe | `KWHO` | become |
| `SKHRO` | run | `KPHRO` | make |
| `PWHO` | take | `TPWO` | find |
| `STPWO` | give | `STWO` | use |
| `TKWRO` | work | `SKPO` | need |
| `STKWO` | remember | `SKHO` | understand |
| `STKRO` | try | `TKPRO` | expect |

The allocation retains every key from the original pre-number-bar mnemonic in
31 of 38 starters. Five are exact apart from the required `O`: hold `TKHO`,
make `KPHRO`, use `STWO`, need `SKPO`, and understand `SKHO`. The remaining
starters were selected globally rather than independently so the entire phrase
universe stays collision-free while no starter exceeds five keys.

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
| `-SZ` | out | `-GT` | with |
| `-RPB` | he (auxiliary inversion) | `-RB` | she (auxiliary inversion) |
| `-P` | it | `-S` | us |
| `-R` | her | `-Z` | his |
| `-PZ` | its | `-RZ` | your |
| `-FB` | of | `-TS` | they (auxiliary inversion) |
| `-PL` | my | `-PLS` | myself |
| `-PLT` | me | `-RT` | that |

Per-verb allowlists in `phrasing.json` remove ungrammatical or low-value
combinations. Individual tails can also carry stem and form allowlists. The
auxiliary inversion tails use `-RPB` for *he* and `-RB` (the `SH` chord) for
*she*. The neighboring shapes make *he* feel like an inversion of *she* while
remaining disjoint from every IV form. `-TS` adds plural *they* inversions for
the be, have, and do stems, while `-PZ` supplies possessive *its* wherever the
other possessive tails are allowed. `with` moves to `-GT`, retaining the
ending of Phoenix's `WEUGT` outline. The selected starters keep the entire
allowlisted universe free of hard Phoenix and internal collisions.

Examples:

| Outline | Output |
| --- | --- |
| `SKPWO-B` | is a |
| `SKPWO-RPB` | is he |
| `SKPWO-RB` | is she |
| `SKPWOE-RPBD` | were he |
| `SKPWOERBD` | were she |
| `SKPWOETS` | are they |
| `TKPOETS` | have they |
| `STKHOETS` | do they |
| `SKPWO-GT` | is with |
| `SKPWAO-BD` | could be a |
| `STHO-P` | says it |
| `STHO*-P` | saying it |
| `TWHOU-RT` | to think that |
| `KPROU-P` | to keep it |

## Final-verb phrases

FV outlines retain the existing grammar:

```text
starter + operator + structure + ender
```

The starter assignments follow Jeff phrasing's unique-starter idea. They are
allocated globally because each starter expands into the complete operator,
structure, ender, and contraction bank. Every selected FV bank is completely
empty in Phoenix.

### FV starters

| Starter | Subject / agreement |
| --- | --- |
| `SWHR` | I / first singular |
| `SKPWR` | you / plural agreement |
| `KWHR` | he / third singular |
| `SKWHR` | she / third singular |
| `STPWH` | it / third singular |
| `STPWR` | we / plural agreement |
| `TKWH` | they / plural agreement |
| `STKWH` | this / third singular |
| `STWH` | that / third singular |
| `STWHR` | there / third singular |
| `TKPHR` | there / plural agreement |
| `SKPWH` | and / plural agreement |

The `and` starter keeps Phoenix's mnemonic `SKP` core but cannot use `SKP*`:
`*` is already the FV negative operator, so a starred starter would collapse
ordinary and negative forms. `SKPWH` is the nearest audited variant whose full
FV bank stays distinct from Phoenix words, commands, and the NV bank. Plural
agreement gives the useful base forms `and are`, `and do`, and `and go`.

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
| `STPWH-B` | it is |
| `SKWHR*U-RLT` | she doesn't tell |
| `STWHAOU` | that'll |
| `SKPWH-G` | and go |
| `SKPWH*-G` | and do not go |

## Nonverb phrases

NV prefixes are mnemonic chords selected as a global set. They do not need a
family marker because no selected NV outline overlaps IV, FV, or a hard Phoenix
entry.

| Outline prefix | Text | Outline prefix | Text |
| --- | --- | --- | --- |
| `TWH` | with | `STP` | at |
| `STP*` | it | `SKH*` | off |
| `PR*U` | up | `SK*` | can |
| `SKP*` | and | `SKWRO*` | just |
| `WHR` | all | `TPW*` | if |
| `STPHRO` | only | `PWH` | but |
| `STHA` | that | `TKPO*` | for |
| `WRO*` | of | `TKPWH*` | anything |
| `SP*` | as | `SRAO*` | even |
| `WHR*` | will | `SKH` | off (compatibility alias) |

NV tails are defined in `phrasing.json`, with explicit per-prefix lists. Every
primary prefix exposes all 21 tails: the layout is a complete 19 by 21 Cartesian
product, and grammaticality is not a filter. The prefix assignments were chosen
globally so all 399 combinations have distinct outlines and avoid Phoenix words
and commands. The `SKP*` bank intentionally replaces 14 Phoenix phrases with
the systematic NV meanings, while retaining no collisions with Phoenix words or
commands. Most tails are right-hand mnemonic chords. The exceptions
introduced for the new families are
`EUF` for `if`, `-F` for `of`, `-FP` for `off`, `-RPB` for `he`, and `-RB` for
`she`; `-BT` is the shared `at` tail. The `off` tail is omitted after prefixes
containing `-F`, where it would collapse with the existing `-P` (`it`) tail.
The `at` tail is available after every prefix. Subject fragments include
`SK*RPB` (`can he`), `SK*RB` (`can she`), `TPW*RPB` (`if he`), and
`TPW*RB` (`if she`). The `WHR*` bank also includes continuations such as
`WHR*U` (`will you`), `WHR*SZ` (`will they`), `WHR*P` (`will it`), and `WHR*EUF`
(`will if`).

Examples:

| Outline | Output |
| --- | --- |
| `TWH-B` | with a |
| `TKPWH*BL` | anything like |
| `SP*EUF` | as if |
| `SRAO*EUF` | even if |
| `SRAO*FP` | even off |
| `SRAO*GT` | even though |
| `SRAO*BT` | even at |
| `WRO*BT` | of at |
| `WHR*F` | will of |
| `SK*RPB` | can he |
| `SKP*B` | and a |
| `SKP*G` | and can |
| `TPW*RB` | if she |
| `SP*F` | as of |
| `SKH-T` | off the |
| `SKH*U` | off you |
| `SKH*F` | off of |
| `PR*URPB` | up he |
| `PR*URB` | up she |
