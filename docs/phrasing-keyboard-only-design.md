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

The checker understands Plover number-bar aliases, ignores multi-stroke
dictionary entries because every generated phrase is one stroke, and fails on
either an internal phrase collision or a Phoenix collision. It checks the full
FV Cartesian product, including combinations the formatter later rejects, so a
clean result is stronger than checking only phrases which currently emit text.

## Family summary

| Family | Layout | Purpose |
| --- | --- | --- |
| IV | `# + verb stem + form + tail` | verb-first phrases such as `say it` |
| FV | `unique subject starter + operator + structure + ender` | subject-first phrases such as `she has gone` |
| NV | `#O + unique prefix + tail` | nonverb phrases such as `with a` |

`#` and `#O` are keys in the phrase chord, not mode-switch strokes. FV uses
Phoenix-empty left-hand starter banks. Contracted FV outlines add `#`; their
left-hand banks remain distinct from every IV and NV bank.

## Initial-verb phrases

IV outlines are constructed as:

```text
# + stem + form + right-hand tail
```

### IV stems

The table shows the stem component. The complete outline always includes `#`.

| Stem | Verb | Stem | Verb |
| --- | --- | --- | --- |
| `PW` | be | `H` | have |
| `S` | see | `ST` | say |
| `TH` | think | `THR` | tell |
| `KH` | catch | `HR` | look |
| `TKH` | hold | `SHR` | sell |
| `SPHR` | spell | `PHR` | pull |
| `P` | put | `KP` | keep |
| `KHR` | call | `TK` | do |
| `TKPW` | go | `W` | want |
| `SK` | ask | `SP` | happen |
| `SW` | feel | `K` | come |
| `TPH` | know | `TKPWH` | get |
| `PWHR` | believe | `KW` | become |
| `R` | run | `KPHR` | make |
| `PH` | take | `TP` | find |
| `STP` | give | `STW` | use |
| `WR` | work | `SKP` | need |
| `SKW` | remember | `SKH` | understand |
| `TR` | try | `TKP` | expect |

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

The star is reserved for the IV progressive form. It keeps IV progressive
outlines separate from the `#O` NV family.

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

Per-verb allowlists in `phrasing.json` remove ungrammatical combinations. In
particular, `go the`, `go it`, and `come the` are omitted because their number
bar chords are Phoenix numeric aliases and are not useful standalone phrases.

Examples:

| Outline | Output |
| --- | --- |
| `#PW-B` | is a |
| `#PWA-BD` | could be a |
| `#ST-P` | says it |
| `#ST*-P` | saying it |
| `#THU-RT` | to think that |
| `#KPU-P` | to keep it |

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

Adding `#` requests the grammatically valid contracted version of an FV
outline. Long form remains the default.

Examples:

| Outline | Output |
| --- | --- |
| `SKWHR-B` | she is |
| `SKWHRE-G` | she is going |
| `SWHR-FPBG` | I have thought |
| `TKWHAO-RLT` | they will tell |
| `#SKWHR*RLT` | she doesn't tell |
| `#STWHAO` | that'll |

## Nonverb phrases

Every NV prefix includes the `#O` family marker in its stored outline.

| Outline prefix | Text | Outline prefix | Text |
| --- | --- | --- | --- |
| `#WO` | with | `#TO` | at |
| `#TO*` | it | `#AOUF` | off |
| `#OUP` | up | `#KO` | can |
| `#SKWRO` | just | `#HRO` | all |
| `#TPO` | if | `#TPHRO` | only |
| `#PWO` | but | `#THAO` | that |
| `#TPAO` | for | `#OF` | of |
| `#TKPWHO*` | anything | `#SO*` | as |
| `#SRAO*E` | even |  |  |

NV tails are defined in `phrasing.json`. They include `a`, `an`, `like`,
`the`, pronouns, `that`, `can`, `if`, `though`, and `else`, with per-prefix
allowlists. The `can` tail is `-G`; `-BG` would make `as can` collide with
Phoenix's `10:00` outline.

Examples:

| Outline | Output |
| --- | --- |
| `#WO-B` | with a |
| `#TKPWHO*-BL` | anything like |
| `#SO*-F` | as if |
| `#SRAO*E-GT` | even though |
