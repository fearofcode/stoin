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

## Optional Pedal Namespace

`--phrase-toggle KEY` enables a separate phrase namespace while keeping the
same phrasing data. The key is intended for a pedal remapped to something like
`F13`. When used alone, this pedal selects all phrase families for backward
compatibility.

`--nonverb-phrase-toggle KEY` adds a second phrase pedal for the `NV` family.
When both pedals are configured, `--phrase-toggle` selects only verb families
(`IV` and `FV`) and `--nonverb-phrase-toggle` selects only `NV`. Startup rejects
duplicate keycodes so the two pedals cannot silently collapse into one
namespace.

When no phrase toggle is used, phrase matching keeps the keyboard-only behavior:
phrases are checked before loaded dictionaries. When a phrase toggle is used,
ordinary strokes skip phrase matching and use the dictionaries. A stroke is a
phrase stroke if the pedal is down during any part of chord gathering; the pedal
does not need to remain held through release. Phrase strokes print the usual
`[phrase]` trace marker. A phrase-mode miss emits raw steno instead of falling
through to a dictionary word, except star-only strokes, which use the dictionary
fallback path and trace as `[phase fallback]`.

## IV Set 1

Initial-verb phrases are generated assignments. The trainer reads the phrasing
sections directly and presents selectable IV/FV/NV banks, so adding a stem,
tail, ender, or flag should not require listing every drill prompt by hand.

### IV Verb Stems

| Stem | Verb |
| --- | --- |
| `PW` | to be |
| `H` | to have |
| `S` | to see |
| `ST` | to say |
| `TH` | to think |
| `THR` | to tell |
| `TKH` | to hold |
| `SHR` | to sell |
| `SPHR` | to spell |
| `KP` | to keep |
| `KHR` | to call |

### IV Flags

| Flag | Meaning |
| --- | --- |
| empty | third-person present: `is`, `has`, `calls`, `says`, `thinks`, `tells`, `holds`, `sells`, `spells`, `keeps` |
| `D` | past: `was`, `had`, `called`, `said`, `thought`, `told`, `held`, `sold`, `spelled`, `kept` |
| `E` | base/non-third present or imperative: `are`, `have`, `call`, `say`, `think`, `tell`, `hold`, `sell`, `spell`, `keep` |
| `ED` | plural past for `PW`: `were` |
| `G` | present participle/gerund for stems that define one: `calling`, `saying`, `thinking`, `telling`, `holding`, `selling`, `spelling`, `keeping` |
| `U` | infinitive with `to`: `to be`, `to have`, `to call`, `to say`, `to think`, `to tell`, `to hold`, `to sell`, `to spell`, `to keep` |
| `A` | modal base: `can` + base verb |
| `AD` | modal past: `could` + base verb |

`E` names the base-form slot, not the word `are`; `PW` happens to surface
that slot as `are`. `U` names the infinitive-with-`to` slot for IV only; the FV
grammar does not generate forms like `he to be`. `F` is reserved for `have` /
perfect work outside IV. `A` follows the FV can/could mnemonic.

IV stems should avoid right-hand `G` because `-G` is the IV gerund/progressive
flag. A verb whose best mnemonic needs right-hand `G` should either choose a
different IV stem or omit the `-G` form for that stem. FV enders may use `G`
normally because FV progressive uses `E`.

### IV Tails

| Present Tail | Past Tail | Object |
| --- | --- | --- |
| `B` | `BD` | a |
| `T` | `TD` | the |
| `P` | `PD` | it |
| `S` | `SD` | us |
| `RT` | `RTD` | that |

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
| `RPBTS` / `RPBTSD` | keep | kept |
| `RLT` / `RLTD` | tell | told |
| `FPL` / `FPLD` | hold | held |
| `LS` / `LSD` | sell | sold |
| `PLS` / `PLSD` | spell | spelled |
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
| `-S` | `*` us | `TW`, `SRAO*E` |
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
| `PWU-B` | to be a |
| `PWE-BD` | were a |
| `PWA-B` | can be a |
| `PWA-BD` | could be a |
| `PW-RTD` | was that |
| `H-B` | has a |
| `H-BD` | had a |
| `HE-B` | have a |
| `HU-B` | to have a |
| `HA-P` | can have it |
| `HA-PD` | could have it |
| `H-RTD` | had that |
| `ST-P` | says it |
| `ST-PD` | said it |
| `STE-P` | say it |
| `STU-P` | to say it |
| `ST-PG` | saying it |
| `STA-P` | can say it |
| `TH-P` | thinks it |
| `TH-PD` | thought it |
| `THE-P` | think it |
| `THU-P` | to think it |
| `TH-PG` | thinking it |
| `THA-P` | can think it |
| `THR-S` | tells us |
| `THR-SD` | told us |
| `THRE-S` | tell us |
| `THRU-S` | to tell us |
| `THR-GS` | telling us |
| `THRA-S` | can tell us |
| `TKH-P` | holds it |
| `TKH-PD` | held it |
| `TKHE-P` | hold it |
| `TKHU-P` | to hold it |
| `TKH-PG` | holding it |
| `TKHA-P` | can hold it |
| `SHR-S` | sells us |
| `SHR-SD` | sold us |
| `SHRE-S` | sell us |
| `SHRU-S` | to sell us |
| `SHR-GS` | selling us |
| `SHRA-P` | can sell it |
| `SPHR-S` | spells us |
| `SPHR-SD` | spelled us |
| `SPHRE-S` | spell us |
| `SPHRU-S` | to spell us |
| `SPHR-GS` | spelling us |
| `SPHRA-P` | can spell it |
| `KP-P` | keeps it |
| `KP-PD` | kept it |
| `KPE-P` | keep it |
| `KPU-P` | to keep it |
| `KP-PG` | keeping it |
| `KP-S` | keeps us |
| `KP-SD` | kept us |
| `KPE-S` | keep us |
| `KPU-S` | to keep us |
| `KP-GS` | keeping us |
| `KPA-P` | can keep it |
| `KHR-B` | calls a |
| `KHRE-B` | call a |
| `KHRU-B` | to call a |
| `KHRA-P` | can call it |
| `KHRA-PD` | could call it |
| `KHR-PG` | calling it |
| `KHR-RTD` | called that |

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
| `SKWHR-BS` | she says |
| `SKWHR-BSZ` | she said |
| `SKWHR-RLT` | she tells |
| `SKWHR-RLTD` | she told |
| `SKWHR-FPL` | she holds |
| `SKWHR-FPLD` | she held |
| `SKWHR-LS` | she sells |
| `SKWHR-LSD` | she sold |
| `SKWHR-PLS` | she spells |
| `SKWHR-PLSD` | she spelled |
| `SKWHR-RPBTS` | she keeps |
| `SKWHR-RPBTSD` | she kept |
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
| `TW-S` | with us |
| `TW-RT` | with that |
| `TKPWH*-RT` | anything that |
| `TKPWH*-F` | anything else |
| `S*-F` | as if |
| `S*-GT` | as though |
| `SRAO*E-S` | even us |
| `SRAO*E-B` | even a |
| `SRAO*E-F` | even if |
| `SRAO*E-GT` | even though |
| `STPHEFD` | instead of |
