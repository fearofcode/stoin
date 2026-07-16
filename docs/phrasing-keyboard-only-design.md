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

## Compatibility with Lapwing and Magnum

Phoenix is the collision target for the checked-in assignments. The same
audit against `lapwing-base.json` finds 50 collisions, all of which are phrases
Stoin can emit: 36 IV and 14 NV. FV is already fully disjoint from both Phoenix
and Lapwing.

### Lapwing conflicts

The IV conflicts occupy 17 starter banks:

| Phrase | Outline | Lapwing translation |
| --- | --- | --- |
| catching an | `KPHO*PB` | c'mon |
| catches an | `KPHOPB` | common |
| caught the | `KPHOTD` | commodity |
| to catch the | `KPHOUT` | come out |
| makes her | `KPHROR` | explore |
| made her | `KPHRORD` | explored |
| makes your | `KPHRORZ` | explores |
| could keep her | `KPRAORD` | extraordinary |
| keep his | `KPROEZ` | compromise |
| kept her | `KPRORD` | extraordinary |
| becomes a | `KWHOB` | yob |
| became an | `KWHOPBD` | beyond |
| becomes her | `KWHOR` | yore |
| run us | `SKHROES` | as close as |
| runs it | `SKHROP` | scallop |
| calls her | `SKPHOR` | and more |
| needs her | `SKPOR` | and/or |
| spell it | `SPWROEP` | entropy |
| spell an | `SPWROEPB` | enthrone |
| spells it | `SPWROP` | entropy |
| say an | `STHOEPB` | shown to |
| try my | `STKROEPL` | syndrome |
| tries my | `STKROPL` | syndrome |
| happening that | `STPO*RT` | so forth |
| can hold an | `TKHAOPB` | done that |
| can hold us | `TKHAOS` | does that |
| holds an | `TKHOPB` | done this |
| to hold us | `TKHOUS` | how does |
| comes her | `TKHROR` | dolor |
| gets an | `TKPHOPB` | demon |
| have us | `TKPOES` | depose |
| have his | `TKPOEZ` | depose |
| has that | `TKPORT` | deport |
| had that | `TKPORTD` | deported |
| finds an | `TPWOPB` | been to |
| tell an | `TWHROEPB` | thrown |

The NV conflicts occupy six prefix banks:

| Phrase | Outline | Lapwing translation |
| --- | --- | --- |
| of an | `KWH-PB` | why know |
| of her | `KWH-R` | why are |
| of us | `KWH-S` | why is |
| of the | `KWH-T` | why the |
| of you | `KWHU` | why you |
| it her | `SKP-R` | and are |
| it the | `SKP-T` | and the |
| it a | `SKPB` | and be |
| it if | `SKPF` | and have |
| it you | `SKPU` | and you |
| if that | `SKW-RT` | sqrt |
| all the | `STH-T` | is this the |
| at her | `STP-R` | is for |
| up like | `TRBL` | attributable |

### Example Lapwing-compatible starter remap

One way to eliminate all 50 conflicts without introducing special cases is to
remap the complete affected starter banks. The following is one audited global
allocation; it is guidance, not the checked-in layout. Applied as a set, it is
free of Phoenix, Lapwing, and internal structural collisions. It moves 1,963
phrase outlines, which is why it should not be adopted casually.

| Family | Bank | Current starter | Example replacement | Key change |
| --- | --- | --- | --- | --- |
| IV | catch | `KPHO` | `TKPWHO` | add `T-`, `W-` |
| IV | make | `KPHRO` | `SKPHRO` | add `S-` |
| IV | keep | `KPRO` | `KPWHRO` | add `W-`, `H-` |
| IV | become | `KWHO` | `STKPWHO` | add `S-`, `T-`, `P-` |
| IV | run | `SKHRO` | `STKHRO` | add `T-` |
| IV | call | `SKPHO` | `SKPWHO` | add `W-` |
| IV | need | `SKPO` | `STKPWRO` | add `T-`, `W-`, `R-` |
| IV | spell | `SPWRO` | `SPWHRO` | add `H-` |
| IV | say | `STHO` | `STPWHO` | add `P-`, `W-` |
| IV | try | `STKRO` | `STKWRO` | add `W-` |
| IV | happen | `STPO` | `STKPRO` | add `K-`, `R-` |
| IV | hold | `TKHO` | `STKPHRO` | add `S-`, `P-`, `R-` |
| IV | come | `TKHRO` | `TKWHRO` | add `W-` |
| IV | get | `TKPHO` | `STKPHO` | add `S-` |
| IV | have | `TKPO` | `STKPWHRO` | add `S-`, `W-`, `H-`, `R-` |
| IV | find | `TPWO` | `STPWRO` | add `S-`, `R-` |
| IV | tell | `TWHRO` | `TPWHRO` | add `P-` |
| NV | of | `KWH` | `STKWHR` | add `S-`, `T-`, `R-` |
| NV | it | `SKP` | `SKPH` | add `H-` |
| NV | if | `SKW` | `SKPWHR` | add `P-`, `H-`, `R-` |
| NV | all | `STH` | `SPHR` | remove `T-`; add `P-`, `R-` |
| NV | at | `STP` | `STKPW` | add `K-`, `W-` |
| NV | up | `TR` | `TWHR` | add `W-`, `H-` |

To carry out this style of remap, edit only these fields in `phrasing.json`:

- For IV, replace the matching `stroke` values under
  `initial_verbs.stems`. Leave each stem's `forms`, `tails`, and tail allowlist
  unchanged.
- For NV, replace the matching `stroke` values under `nonverbs.prefixes`.
  Leave the shared `nonverbs.tails` assignments and prefix allowlists
  unchanged.
- Do not edit `final_verbs`; FV has no Phoenix or Lapwing collision.

The example allocation is global. Applying only some rows can reuse a bank
reserved by another row and invalidate the audit. After any edit, update the
outline expectations in `tests/test_steno.c`, update the starter tables in this
document, and run both audits plus the test suite:

```sh
scripts/check-phrasing-collisions.py phrasing.json ~/Downloads/phoenix.json
scripts/check-phrasing-collisions.py phrasing.json lapwing-base.json
make test
```

A lower-churn implementation could instead add explicit per-outline exclusions
and alternate aliases for the 50 conflicts. The current schema has no such
exception facility: adding aliases alone would make the alternate strokes
typeable, but the original generated outlines would still conflict underneath
Lapwing's dictionary overrides.

### Magnum summary

Magnum is a substantially larger compatibility target. The same snapshot audit
finds 5,791 conservative structural collisions and 5,177 collisions among
phrases Stoin actually emits: 941 IV, 4,153 FV, and 83 NV. Representative
conflicts include:

| Stoin phrase | Outline | Magnum translation |
| --- | --- | --- |
| she will | `SKWHRAO` | she doesn't |
| they are | `TKWHE` | do you know when he |
| finds his | `TPWOZ` | about to see |
| with it | `TW-P` | the only person |
| it cannot have | `KPWHA*F` | it can't have |

A starter-only search does not provide enough clean banks for Magnum: under the
current operator, structure, tail, and ender assignments, only one left-hand FV
bank is clean, and it works only for two restricted `there` rows. Supporting
Magnum strictly would therefore require a broader grammar-layout redesign or a
large exception map rather than the Lapwing remap above.

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
| `SKPWO` | be | `TKPO` | have |
| `SPWHO` | see | `STHO` | say |
| `TWHO` | think | `TWHRO` | tell |
| `KPHO` | catch | `WHRO` | look |
| `TKHO` | hold | `STHRO` | sell |
| `SPWRO` | spell | `KPWRO` | pull |
| `STPRO` | put | `KPRO` | keep |
| `SKPHO` | call | `STKHO` | do |
| `TWRO` | go | `SKWHO` | want |
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
| `SKPWO-B` | is a |
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
