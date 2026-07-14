# Phrasing Reference

## Global Rules

| Topic | Rule |
| --- | --- |
| Activation | `--phrase-toggle KEY` selects the phrase namespace for a stroke |
| Match order | while the phrase pedal is active, phrase matching replaces dictionary lookup |
| Families | `IV`, `FV`, `NV` |
| Contractions | only when `#` is pressed |
| Default output | long forms, never contractions |
| Follow-ons | none |
| Custom chunks | default config loads `stoin-custom.json`; when overriding config, include it after base dictionaries with `--dictionary` |

## Optional Pedal Namespace

`--phrase-toggle KEY` enables a separate phrase namespace. The key is intended
for a pedal remapped to something like `F13`. When it is the only phrase pedal,
it selects all phrase families for backward compatibility.

`--nonverb-phrase-toggle KEY` adds a separate pedal for the `NV` family. When
both phrase pedals are configured, `--phrase-toggle` selects only `IV` and `FV`,
while `--nonverb-phrase-toggle` selects only `NV`. Holding both selects all
families. The pedal keycodes must be distinct.

Ordinary strokes use the main dictionary stack; a stroke is a phrase stroke if
either applicable pedal is down during any part of chord gathering, so the
pedal may be released before the chord.

Phrase strokes print the `[phrase]` trace marker. A phrase miss emits raw steno
instead of falling through to a dictionary word, except star-only strokes,
which use the dictionary fallback path and trace as `[phase fallback]`.

## Modal Dictionary Pedal

A second, separately loaded dictionary can be assigned to a momentary pedal.
It is not merged into the main dictionary stack. Configure its path in JSON:

```json
{
  "modal_dictionary": "/absolute/path/to/magnum.json"
}
```

The equivalent command-line path is `--modal-dictionary PATH`. Assign the
pedal with `--modal-dictionary-toggle F14` (or `--modal-toggle F14`). The verb,
nonverb, and modal pedals must all resolve to different keycodes.

While the modal pedal is active, lookup uses only the modal dictionary. It does
not fall through to IV/FV phrasing or the main dictionary, and a miss emits raw
steno. Modal state is latched for a chord in the same way as phrase state.
Consecutive modal strokes form one run; normal or phrase input closes the run,
so outlines cannot cross a dictionary-mode boundary. A modal history-changing
command also starts a new run when it cannot preserve an equivalent stroke
sequence.

For plain-text modal entries, Stoin evaluates complete segmentations of the
run. It first minimizes untranslated strokes, then prefers the segmentation
that emits the most whitespace-delimited words. Ties prefer fewer dictionary
segments and then a longer final segment. Thus Magnum's `but if` plus `I can`
wins over its conflicting two-stroke `glyphic` entry. If any competing entry
uses a Plover formatting or command value, Stoin retains ordinary longest-match
behavior rather than trying to concatenate or score side effects.

Outside modal mode, an exact single stroke that the main stack cannot translate
is delegated to the modal dictionary. This supports supplemental words such as
Magnum `STPHULGDZ` -> `snuggling` without enabling Magnum's multi-stroke
boundary conflicts during normal typing. Main-stack entries always win, and
the fallback never considers a multi-stroke modal outline.

## Why Keep Verb Phrasing

The production IV/FV grammar still supplies natural everyday phrases that are
not direct entries in the checked Magnum or Phoenix dictionaries. These
examples were checked as exact, case-sensitive dictionary values; they may of
course be written more slowly by composing ordinary dictionary strokes.

| Stoin outline | Output | Family / form |
| --- | --- | --- |
| `SA-P` | can see it | IV modal + object |
| `TH-RGT` | thinking that | IV present participle + complement |
| `KPU-P` | to keep it | IV infinitive + object |
| `#SWR*E-BTS` | I'm not saying that | FV contracted negative progressive |
| `SWRE-PBGTD` | I was thinking that | FV past progressive |
| `#SWRE-FPBG` | I've been thinking | FV contracted perfect progressive |
| `#TWRAO-RTS` | we'll try to | FV contracted `will` modal |
| `KPWRO-PBT` | you should know that | FV `should` modal |
| `#SKWHR*-FBTS` | she hasn't said that | FV contracted negative perfect |
| `#KPWRA*-PBLGTD` | you couldn't find that | FV contracted negative past modal |

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
| `KH` | to catch |
| `HR` | to look |
| `TKH` | to hold |
| `SHR` | to sell |
| `SPHR` | to spell |
| `PHR` | to pull |
| `P` | to put |
| `KP` | to keep |
| `KHR` | to call |
| `TK` | to do |
| `TKPW` | to go |
| `W` | to want |
| `SK` | to ask |
| `SP` | to happen |
| `SW` | to feel |
| `K` | to come |
| `TPH` | to know |
| `TKPWH` | to get |
| `PWHR` | to believe |
| `KW` | to become |
| `R` | to run |
| `KPHR` | to make |
| `PH` | to take |
| `TP` | to find |
| `STP` | to give |
| `STW` | to use |
| `WR` | to work |
| `SKP` | to need |
| `SKW` | to remember |
| `SKH` | to understand |
| `TR` | to try |
| `TKP` | to expect |

### IV Flags

| Flag | Meaning |
| --- | --- |
| empty | third-person present: `is`, `has`, or the stem's third-person form |
| `D` | simple past: `was`, `had`, or the stem's past form |
| `E` | base/non-third present or imperative: `are`, `have`, or the stem's base form |
| `ED` | plural past for `PW`: `were` |
| `G` | present participle/gerund for stems that define one |
| `U` | infinitive with `to` |
| `A` | modal base: `can` + base verb |
| `AD` | modal past: `could` + base verb |

`E` names the base-form slot, not the word `are`; `PW` happens to surface
that slot as `are`. `U` names the infinitive-with-`to` slot for IV only; the FV
grammar does not generate forms like `he to be`. `F` is reserved for `have` /
perfect work outside IV. `A` follows the FV can/could mnemonic.

IV stems stay on the left hand so their bits cannot disappear into right-hand
tails. `Make` uses `KPHR`, which remains distinct from both `KP` (`keep`) and
`KHR` (`call`). Planned `KH` for `run` is already the `catch` stem, so `run`
uses the free, mnemonic left-hand `R` instead. FV enders may use the full right
hand because FV progressive uses `E` instead of `-G`.

### IV Tails

| Present Tail | Past Tail | Object |
| --- | --- | --- |
| `B` | `BD` | a |
| `PB` | `PBD` | an |
| `BL` | `BLD` | like |
| `T` | `TD` | the |
| `LT` | `LTD` | at |
| `RP` | `RPD` | up |
| `P` | `PD` | it |
| `S` | `SD` | us |
| `R` | `RD` | her |
| `Z` | `ZD` | his |
| `FB` | `FBD` | of |
| `PL` | `PLD` | my |
| `PLS` | `PLSD` | myself |
| `PLT` | `PLTD` | me |
| `RT` | `RTD` | that |

An IV stem may declare a `tails` array containing tail IDs from this table.
When present, only those verb-tail combinations translate and appear in the
trainer; omitting the field preserves the original all-tail behavior. The
follow-on verbs use allowlists to exclude combinations such as `happens me`,
`comes us`, `becomes at`, and `uses of`.

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
| `T` | it |
| `TWR` | we |
| `TWH` | they |
| `STKH` | this |
| `STWH` | that |
| `THR` | there (default/singular agreement) |
| `STPHR` | there (plural agreement; shown as `there (plural)` in the trainer) |

`STKH` and `STWH` retain the established Jeff full-form mnemonics for `this`
and `that`; both use third-singular agreement. The project's shorter `THR`
starter uses default third-singular agreement, while the `P` in Jeff's
`STPHR` outline marks plural agreement. Thus `THR` uses `is`, `has`, `does`,
and `was`, while `STPHR` uses `are`, `have`, `do`, and `were`. The separate
trainer label makes that distinction visible without changing the translated
word `there`.

Unlike the personal-pronoun starters, `there` is not grammatical with every
FV ender. Both agreement forms use the following restricted family; plural
`STPHR` additionally permits the collision-free explicit `B` and `BD` be
enders:

| Ender family | There phrase family |
| --- | --- |
| empty / `D` | auxiliary-only forms |
| `B` / `BD` | are / were (`STPHR` only) |
| `BT` / `BTD` | be a / was a |
| `TS` / `TSD` | have to / had to |
| `G` / `GD`, `GT` / `GTD` | go / went, go to / went to |
| `L` / `LD` | look / looked |
| `PZ` / `PDZ` | happen / happened |
| `LTS` / `LTSD` | feel like / felt like |
| `BG` / `BGD`, `BGT` / `BGTD` | come / came, come to / came to |
| `RPBG` / `RPBGD`, `RPBGT` / `RPBGTD` | become / became, become a / became a |
| `R` / `RD` | run / ran |
| `RPGT` / `RPGTD` | need to / needed to |
| `GTS` / `GTSD` | get to / got to |
| `RBT` / `RBTD` | take / took (for phrases such as `there takes place`) |
| `RBG` / `RBGD`, `RBGT` / `RBGTD` | work / worked, work on / worked on |

`THR` is also the IV stem for `tell`, and IV lookup has priority when the two
families produce the same bitset. Most conflicts disappear by restricting the
`there` enders, and the remaining colliding combinations are not generated:
`there goes to`, `there comes`, `there has come`, `there runs`, `there ran`,
`there is running`, `there can run`, and `there could run`. The explicit `B`
and `BD` be enders also cannot spell the basic forms because `THR-B` and
`THR-BD` remain `tells a` and `told a`. The empty ender with the `E` structure
provides collision-free `there is` and `there was` instead.

Affirmative contractions are starter-specific. `This` deliberately assigns
only `this'll`, avoiding the uncommon written `this's` and `this'd`. `That`
assigns `that's` for both `that is` and `that has`, plus `that'll` and
`that'd`. Plural `there` assigns `there're`, `there've`, `there'll`, and
`there'd`; default `THR` keeps `there's`, `there'll`, and `there'd`.

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
| `TS` / `TSD` | have to | had to |
| `RP` / `RPD` | do | did |
| `RPT` / `RPTD` | do it | did it |
| `G` / `GD` | go | went |
| `GT` / `GTD` | go to | went to |
| `PB` / `PBD` | know | knew |
| `PBT` / `PBTD` | know that | knew that |
| `L` / `LD` | look | looked |
| `PBG` / `PBGD` | think | thought |
| `PBGT` / `PBGTD` | think that | thought that |
| `RPBTS` / `RPBTSD` | keep | kept |
| `RLT` / `RLTD` | tell | told |
| `RB` / `RBD` | catch | caught |
| `FPL` / `FPLD` | hold | held |
| `LS` / `LSD` | sell | sold |
| `PLS` / `PLSD` | spell | spelled |
| `PL` / `PLD` | pull | pulled |
| `P` / `PD` | want | wanted |
| `PT` / `PTD` | want to | wanted to |
| `RPG` / `RPGD` | need | needed |
| `RPGT` / `RPGTD` | need to | needed to |
| `S` / `SD` | see | saw |
| `BS` / `BSD` | say | said |
| `BTS` / `BTSD` | say that | said that |
| `GS` / `GSD` | get | got |
| `GTS` / `GTSD` | get to | got to |
| `PBLG` / `PBLGD` | find | found |
| `PBLGT` / `PBLGTD` | find that | found that |
| `RT` / `RTD` | try | tried |
| `RTS` / `RTSD` | try to | tried to |
| `PZ` / `PDZ` | happen | happened |
| `LT` / `LTD` | feel | felt |
| `LTS` / `LTSD` | feel like | felt like |
| `BG` / `BGD` | come | came |
| `BGT` / `BGTD` | come to | came to |
| `BL` / `BLD` | believe | believed |
| `BLT` / `BLTD` | believe that | believed that |
| `RPBG` / `RPBGD` | become | became |
| `RPBGT` / `RPBGTD` | become a | became a |
| `R` / `RD` | run | ran |
| `RPBL` / `RPBLD` | make | made |
| `RPBLT` / `RPBLTD` | make a | made a |
| `RBT` / `RBTD` | take | took |
| `GZ` / `GDZ` | give | gave |
| `Z` / `DZ` | use | used |
| `RBG` / `RBGD` | work | worked |
| `RBGT` / `RBGTD` | work on | worked on |
| `RPL` / `RPLD` | remember | remembered |
| `RPLT` / `RPLTD` | remember that | remembered that |
| `RPB` / `RPBD` | understand | understood |
| `RPBT` / `RPBTD` | understand the | understood the |
| `PGS` / `PGSD` | expect | expected |
| `PGTS` / `PGTSD` | expect that | expected that |
| `RBS` / `RBSD` | ask | asked |

The planned `RB` final assignment for `ask` is already the implemented `catch`
ender, so `ask` adds mnemonic `-S` and uses `RBS`. `Use` deliberately has no
ordinary `to` suffix: `used to` requires fixed-form handling rather than normal
verb inflection.

### FV Contraction Patterns

| Pattern | Output |
| --- | --- |
| `#` + present be | starter contraction: `I'm`, `you're`, `he's`, `she's`, `it's`, `we're`, `they're`, `that's`, `there's`, `there're`; affirmative `this is` is unassigned |
| `#` + negative be | `isn't`, `aren't`, `wasn't`, `weren't`; `I am not` becomes `I'm not` |
| `#` + present have/perfect | starter contraction: `I've`, `you've`, `he's`, `she's`, `it's`, `we've`, `they've`, `that's`, `there's`, `there've`; affirmative `this has` is unassigned |
| `#` + past have/perfect | configured starter + `'d`: `I'd`, `you'd`, `he'd`, `she'd`, `it'd`, `we'd`, `they'd`, `that'd`, `there'd`; `this had` is unassigned |
| `#` + negative have/perfect | `haven't`, `hasn't`, `hadn't` |
| `#` + simple negative lexical verb | agreement-aware `don't`, `doesn't`, or `didn't` |
| `#` + `AO` present | starter + `will`: `I'll`, `you'll`, `he'll`, `she'll`, `it'll`, `we'll`, `they'll`, `this'll`, `that'll`, `there'll` |
| `#` + `AO` past | configured starter + `would` contraction: `I'd`, `you'd`, `he'd`, `she'd`, `it'd`, `we'd`, `they'd`, `that'd`, `there'd` |
| `#` + `A*` | `can't` / `couldn't` |
| `#` + `O*` | `shouldn't` |
| `#` + `AO*` | `won't` / `wouldn't` |

Past affirmative `be` contractions remain unassigned because standard English
has no general subject contraction for `was` or `were`.

## NV Set 1

The nonverb pedal selects combinations from an independent prefix-and-tail
bank. The `anything` stem is written in canonical steno order as `TKPWH*`.

### NV Prefixes

| Keys | Output Pattern |
| --- | --- |
| `W` | with `*` |
| `T` | at `*` |
| `SKWR` | just `*` |
| `HR` | all `*` |
| `TP` | if `*` |
| `TPHRO` | only `*` |
| `PW` | but `*` |
| `THA` | that `*` |
| `TPO` | for `*` |
| `OF` | of `*` |
| `TKPWH*` | anything `*` |
| `S*` | as `*` |
| `SRAO*E` | even `*` |

### NV Tails

| Keys | Output | Allowed Prefixes |
| --- | --- | --- |
| `-R` | her | every NV prefix |
| `-B` | a | every NV prefix |
| `-PB` | an | every NV prefix |
| `-BL` | like | `SKWR`, `HR`, `TPHRO`, `PW`, `TKPWH*` |
| `-F` | if | `SKWR`, `TPHRO`, `PW`, `THA`, `TPO`, `S*`, `SRAO*E` |
| `-GT` | though | `PW`, `THA`, `TPO`, `S*`, `SRAO*E` |
| `-LS` | else | `HR`, `TKPWH*` |
| `-P` | it | every NV prefix |
| `-PLT` | them | `W`, `T`, `SKWR`, `TPHRO`, `PW`, `TPO`, `OF`, `S*`, `SRAO*E` |
| `-RT` | that | every NV prefix |
| `-S` | us | `W`, `T`, `SKWR`, `TPHRO`, `PW`, `THA`, `TPO`, `OF`, `S*`, `SRAO*E` |
| `-T` | the | every NV prefix |
| `-Z` | his | every NV prefix |

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
| `THE-FB` | think of |
| `SW-BL` | feels like |
| `SWE-BL` | feel like |
| `SW-BLG` | feeling like |
| `KP-BL` | keeps like |
| `KPHR-BL` | makes like |
| `P-P` | puts it |
| `PE-P` | put it |
| `PU-P` | to put it |
| `THR-S` | tells us |
| `THR-SD` | told us |
| `THRE-S` | tell us |
| `THRU-S` | to tell us |
| `THR-GS` | telling us |
| `THRA-S` | can tell us |
| `KH-P` | catches it |
| `KH-PD` | caught it |
| `KHE-P` | catch it |
| `KHU-P` | to catch it |
| `KH-PG` | catching it |
| `KHA-P` | can catch it |
| `KHE-R` | catch her |
| `KHE-Z` | catch his |
| `KHE-PL` | catch my |
| `KHE-PLS` | catch myself |
| `KHE-PLT` | catch me |
| `HR-P` | looks it |
| `HR-PD` | looked it |
| `HRE-P` | look it |
| `HRU-P` | to look it |
| `HR-PG` | looking it |
| `HRA-P` | can look it |
| `HRELT` | look at |
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
| `PHR-P` | pulls it |
| `PHR-PD` | pulled it |
| `PHRE-P` | pull it |
| `PHRU-P` | to pull it |
| `PHR-PG` | pulling it |
| `PHRA-P` | can pull it |
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
| `SKWHR-L` | she looks |
| `SKWHR-LD` | she looked |
| `SKWHR-BS` | she says |
| `SKWHR-BSD` | she said |
| `SKWHR-RLT` | she tells |
| `SKWHR-RLTD` | she told |
| `SKWHR-RB` | she catches |
| `SKWHR-RBD` | she caught |
| `SKWHR-FPL` | she holds |
| `SKWHR-FPLD` | she held |
| `SKWHR-LS` | she sells |
| `SKWHR-LSD` | she sold |
| `SKWHR-PLS` | she spells |
| `SKWHR-PLSD` | she spelled |
| `SKWHR-PL` | she pulls |
| `SKWHR-PLD` | she pulled |
| `SKWHR-RPBTS` | she keeps |
| `SKWHR-RPBTSD` | she kept |
| `SKWHRAO-G` | she will go |
| `SKWHRAO*G` | she will not go |
| `SKWHREG` | she is going |
| `SKWHR-FG` | she has gone |
| `STKH-B` | this is |
| `STKHAO*` | this will not |
| `STWH-B` | that is |
| `STWHAO-G` | that will go |
| `THRE` | there is |
| `THRED` | there was |
| `THR*ED` | there was not |
| `THRAO*` | there will not |
| `THRE-F` | there has been |
| `STPHR-B` | there are |
| `STPHR-BD` | there were |
| `STPHRAO*` | there will not |

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
| `#TWH-FRLTD` | they'd told |
| `#TWHE-FRLTD` | they'd been telling |
| `#TWHAO-RLTD` | they'd tell |
| `#TWHAO-FRLTD` | they'd have told |
| `#TWH*RLTD` | they didn't tell |
| `#SKWHR*RLT` | she doesn't tell |
| `#STKHAO` | this'll |
| `#STWH-B` | that's |
| `#STWHAO` | that'll |
| `#THRE` | there's |
| `#THR*ED` | there wasn't |
| `#THRAO*` | there won't |
| `#THRE-F` | there's been |
| `#STPHR-B` | there're |
| `#STPHR-F` | there've |
| `#STPHRAO` | there'll |

### NV Samples

| Stroke | Output |
| --- | --- |
| `W-B` | with a |
| `W-PB` | with an |
| `W-T` | with the |
| `W-PLT` | with them |
| `W-S` | with us |
| `W-R` | with her |
| `W-RT` | with that |
| `T-B` | at a |
| `T-PB` | at an |
| `T-R` | at her |
| `T-RT` | at that |
| `SKWR-BL` | just like |
| `HR-LS` | all else |
| `TP-P` | if it |
| `TPHRO-F` | only if |
| `PW-GT` | but though |
| `THA-B` | that a |
| `THA-PB` | that an |
| `THAT` | that the |
| `TPOR` | for her |
| `TPOT` | for the |
| `OFR` | of her |
| `OFP` | of it |
| `OFZ` | of his |
| `TKPWH*-RT` | anything that |
| `TKPWH*-BL` | anything like |
| `TKPWH*-LS` | anything else |
| `S*-F` | as if |
| `S*-PB` | as an |
| `S*-GT` | as though |
| `SRAO*E-S` | even us |
| `SRAO*E-B` | even a |
| `SRAO*E-F` | even if |
| `SRAO*E-GT` | even though |
