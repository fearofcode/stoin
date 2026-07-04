# Keyboard-Only Phrasing Reference

## Global Rules

| Topic | Rule |
| --- | --- |
| Activation | ordinary steno strokes only |
| Match order | phrase matcher wins before loaded dictionaries |
| Families | `IV`, `FV`, `NV` |
| Contractions | only when `#` is pressed |
| Default output | long forms, never contractions |
| Follow-ons | none |
| Custom chunks | default config loads `stoin-custom.json`; when overriding config, include it after base dictionaries with `--dictionary` |

## IV Set 1

Initial-verb phrases are generated assignments. The trainer reads the phrasing
sections directly and presents selectable IV/FV/NV banks, so adding a stem,
tail, ender, or flag should not require listing every drill prompt by hand.

### IV Verb Stems

| Stem | Verb |
| --- | --- |
| `PW` | to be |
| `H` | to have |
| `STKHR` | to call |

### IV Flags

| Flag | Meaning |
| --- | --- |
| empty | third-person present: `is`, `has`, `calls` |
| `D` | past: `was`, `had`, `called` |
| `E` | base/non-third present or imperative: `are`, `have`, `call` |
| `ED` | plural past for `PW`: `were` |
| `G` | present participle/gerund for stems that define one: `calling` |

`E` names the base-form slot, not the word `are`; `PW` happens to surface
that slot as `are`. `F` is reserved for `have` / perfect work outside IV, and
`U` is intentionally left unused because it collides easily with ordinary word
outlines.

### IV Tails

| Present Tail | Past Tail | Object |
| --- | --- | --- |
| `B` | `BD` | a |
| `T` | `TD` | the |
| `P` | `PD` | it |
| `RT` | `RTD` | that |

### IV Exact Rows

| Stroke | Output |
| --- | --- |
| `PW-B` | is a |
| `PW-BD` | was a |
| `PWE-B` | are a |
| `PWE-BD` | were a |
| `PW-T` | is the |
| `PW-TD` | was the |
| `PWE-T` | are the |
| `PWE-TD` | were the |
| `PW-P` | is it |
| `PW-PD` | was it |
| `PWE-P` | are it |
| `PWE-PD` | were it |
| `PW-RT` | is that |
| `PW-RTD` | was that |
| `PWE-RT` | are that |
| `PWE-RTD` | were that |
| `H-B` | has a |
| `H-BD` | had a |
| `HE-B` | have a |
| `H-T` | has the |
| `H-TD` | had the |
| `HE-T` | have the |
| `H-P` | has it |
| `H-PD` | had it |
| `HE-P` | have it |
| `H-RT` | has that |
| `H-RTD` | had that |
| `HE-RT` | have that |
| `STKHR-B` | calls a |
| `STKHR-BD` | called a |
| `STKHRE-B` | call a |
| `STKHR-BG` | calling a |
| `STKHR-T` | calls the |
| `STKHR-TD` | called the |
| `STKHRE-T` | call the |
| `STKHR-GT` | calling the |
| `STKHR-P` | calls it |
| `STKHR-PD` | called it |
| `STKHRE-P` | call it |
| `STKHR-PG` | calling it |
| `STKHR-RT` | calls that |
| `STKHR-RTD` | called that |
| `STKHRE-RT` | call that |
| `STKHR-RGT` | calling that |

## FV Set 1

Final-verb phrases use:

```text
starter + operator + structure + ender
```

### FV Starters

| Stroke | Starter |
| --- | --- |
| `SWR` | I |
| `KPWR` | you |
| `KWHR` | he |
| `SKWHR` | she |
| `KPWH` | it |
| `TWR` | we |
| `TWH` | they |

### FV Operators

| Keys | Long Output |
| --- | --- |
| empty | plain finite verb |
| `*` | not |
| `A` | can / could |
| `A*` | cannot / could not |
| `O` | should |
| `O*` | should not |
| `AO` | will / would |
| `AO*` | will not / would not |

### FV Structures

| Keys | Long Output |
| --- | --- |
| empty | simple verb |
| `E` | be + present participle |
| `F` | have + past participle |
| `EF` | have been + present participle |

### FV Enders

| Ender | Present Output | Past Output |
| --- | --- | --- |
| empty / `D` | auxiliary only | auxiliary only, past |
| `B` / `BD` | be | was/were |
| `BT` / `BTD` | be a | was/were a |
| `T` / `TD` | have | had |
| `TS` / `TSDZ` | have to | had to |
| `RP` / `RPD` | do | did |
| `RPT` / `RPTD` | do it | did it |
| `G` / `GD` | go | went |
| `GT` / `GTD` | go to | went to |
| `PB` / `PBD` | know | knew |
| `PBT` / `PBTD` | know that | knew that |
| `PBG` / `PBGD` | think | thought |
| `PBGT` / `PBGTD` | think that | thought that |
| `P` / `PD` | want | wanted |
| `PT` / `PTD` | want to | wanted to |
| `RPG` / `RPGD` | need | needed |
| `RPGT` / `RPGTD` | need to | needed to |
| `S` / `SZ` | see | saw |
| `BS` / `BSZ` | say | said |
| `BTS` / `BTSDZ` | say that | said that |
| `GS` / `GSZ` | get | got |
| `GTS` / `GTSDZ` | get to | got to |
| `PBLG` / `PBLGD` | find | found |
| `PBLGT` / `PBLGTD` | find that | found that |
| `RT` / `RTD` | try | tried |
| `RTS` / `RTSDZ` | try to | tried to |

### FV Contraction Patterns

| Pattern | Output |
| --- | --- |
| `#` + present be | starter contraction: `I'm`, `you're`, `he's`, `she's`, `it's`, `we're`, `they're` |
| `#` + negative be | `isn't`, `aren't`, `wasn't`, `weren't`; `I am not` becomes `I'm not` |
| `#` + present have/perfect | starter contraction: `I've`, `you've`, `he's`, `she's`, `it's`, `we've`, `they've` |
| `#` + negative have/perfect | `haven't`, `hasn't`, `hadn't` |
| `#` + `AO` present | starter + `will`: `I'll`, `you'll`, `he'll`, `she'll`, `it'll`, `we'll`, `they'll` |
| `#` + `A*` | `can't` / `couldn't` |
| `#` + `O*` | `shouldn't` |
| `#` + `AO*` | `won't` / `wouldn't` |

Past affirmative be/have/will contractions are unassigned.

## NV Set 1

Non-verb phrase rows are split between generated phrase rows and custom
dictionary rows. The `anything` stem is written in canonical steno order as
`TKPWH*`; it is the same chord as the mnemonic `TPKWH*` idea.

### NV Left-Side Assignments

| Keys | Output Pattern |
| --- | --- |
| `TW` | with `*` |
| `TKPWH*` | anything `*` |
| `S*` | as `*` |
| `SRAO*E` | even `*` |

### NV Right-Hand Assignments

| Keys | Output Pattern | Used With |
| --- | --- | --- |
| `-B` | `*` a | `TW`, `SRAO*E` |
| `-F` | `*` else | `TKPWH*` |
| `-F` | `*` if | `S*`, `SRAO*E` |
| `-GT` | `*` though | `S*`, `SRAO*E` |
| `-PLT` | `*` them | `TW` |
| `-RT` | `*` that | `TW`, `TKPWH*`, `SRAO*E` |
| `-T` | `*` the | `TW` |

### NV Custom Rows

These rows live in `stoin-custom.json`. `TPHORTD` and `STPHEFD` are copied
from the open-source Lapwing dictionary.

| Type | Stroke | Output |
| --- | --- | --- |
| function | `TPHORTD` | in order to |
| function | `STPHEFD` | instead of |

## Samples

### IV Samples

| Stroke | Output |
| --- | --- |
| `PW-B` | is a |
| `PW-T` | is the |
| `PWE-B` | are a |
| `PWE-BD` | were a |
| `PW-RTD` | was that |
| `H-B` | has a |
| `H-BD` | had a |
| `HE-B` | have a |
| `H-RTD` | had that |
| `STKHR-B` | calls a |
| `STKHRE-B` | call a |
| `STKHR-PG` | calling it |
| `STKHR-RTD` | called that |

### FV Long-Form Samples

| Stroke | Output |
| --- | --- |
| `SKWHR-B` | she is |
| `SKWHR-BD` | she was |
| `SKWHR*E` | she is not |
| `SKWHR*ED` | she was not |
| `KWHR-B` | he is |
| `TWH-BD` | they were |
| `SWR-F` | I have |
| `SWR-FD` | I had |
| `KPWR-G` | you go |
| `KPWR-GD` | you went |
| `SKWHR-GTD` | she went to |
| `SKWHR-PBG` | she thinks |
| `SKWHR-PBGD` | she thought |
| `SKWHRAO-G` | she will go |
| `SKWHRAO*G` | she will not go |
| `SKWHREG` | she is going |
| `SKWHR-FG` | she has gone |

### FV Contraction Samples

| Stroke | Output |
| --- | --- |
| `#SKWHR-B` | she's |
| `#SKWHR*E` | she isn't |
| `#SKWHR*ED` | she wasn't |
| `#SKWHRAO-G` | she'll go |
| `#SKWHRAO*G` | she won't go |
| `#SWR-F` | I've |
| `#KWHR-FG` | he's gone |
| `#TWHAO-G` | they'll go |

### NV Samples

| Stroke | Output |
| --- | --- |
| `TW-B` | with a |
| `TW-T` | with the |
| `TW-PLT` | with them |
| `TW-RT` | with that |
| `TKPWH*-RT` | anything that |
| `TKPWH*-F` | anything else |
| `S*-F` | as if |
| `S*-GT` | as though |
| `SRAO*E-B` | even a |
| `SRAO*E-F` | even if |
| `SRAO*E-GT` | even though |
| `STPHEFD` | instead of |
