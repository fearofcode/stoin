# Three-Pedal Phrasing Reference

Draft reference for evaluating the next Stoin phrasing layout.

## Pedals

| Label | Pedal | Shape | Primary output |
| --- | --- | --- | --- |
| `IV` | initial verb | left verb + grammar/state + right tail | `was a`, `believed the`, `could have found it` |
| `FV` | final verb | left starter + grammar/state + right verb | `he is`, `they went`, `I found` |
| `NV` | non-verb | left family + grammar/state + right head | standard-dictionary gap phrase chunks |
| `FO` | no pedal | exact follow-on stroke | modify the previous phrase |

Implementation note: the current Stoin runtime only wires the `IV` pedal path.
Use `--register-pedal initial-verb` for that pedal. `FV`, `NV`, and follow-on
behavior are reference targets, not complete runtime paths yet.

## Shared Grammar/State

These keys are shared by `IV`, `FV`, and `NV`.

| Key | Meaning |
| --- | --- |
| empty | simple present / default state |
| `D` | past / conditional |
| `A` | can / could |
| `O` | should |
| `AO` | will / would |
| `*` | negative |
| `E` | progressive |
| `U` | perfect |
| `EU` | perfect progressive |

Examples:

| Stroke | Output |
| --- | --- |
| `FV[K-G]` | he goes |
| `FV[K-GD]` | he went |
| `FV[K*-G]` | he doesn't go |
| `FV[KA-G]` | he can go |
| `FV[KA-GD]` | he could go |
| `FV[KAO-G]` | he will go |
| `FV[KAO-GD]` | he would go |
| `FV[KE-G]` | he is going |
| `FV[KU-G]` | he has gone |
| `FV[KEU-G]` | he has been going |

## Starter Bank

Used by `FV`.

| Key | Starter | Agreement |
| --- | --- | --- |
| empty | empty starter | third singular |
| `S` | I | first singular |
| `T` | they | plural |
| `K` | he | third singular |
| `P` | it | third singular |
| `W` | we | plural |
| `H` | you | plural |
| `SK` | she | third singular |
| `ST` | that | third singular |
| `TP` | this | third singular |
| `TK` | there | third singular |
| `TKW` | there | plural |
| `PW` | empty plural starter | plural |
| `KW` | who | third singular |
| `TW` | what | third singular |
| `SP` | someone | third singular |
| `SPW` | something | third singular |
| `KP` | everyone | third singular |
| `KPW` | everything | third singular |
| `SKP` | nobody | third singular |
| `SKPW` | nothing | third singular |

## Verb Bank

Used by `IV` on the left hand and `FV` on the right hand.

| Verb | `IV` left key | `FV` right key |
| --- | --- | --- |
| have | `H` | `F` |
| see | `S` | `S` |
| do | `TK` | `RP` |
| go | `TKPW` | `G` |
| be | `PW` | `B` |
| want | `W` | `PBT` |
| say | `ST` | `FB` |
| ask | `SK` | `RB` |
| happen | `SP` | `FP` |
| feel | `SW` | `FL` |
| come | `K` | `FG` |
| know | `TPH` | `PB` |
| think | `TH` | `PBG` |
| look | `TW` | `L` |
| get | `TKPWH` | `PG` |
| believe | `PWHR` | `BL` |
| become | `KW` | `BG` |
| run | `KH` | `R` |
| make | `KPL` | `RPBL` |
| take | `PH` | `RBL` |
| find | `TP` | `FPB` |
| tell | `THR` | `LT` |
| give | `STP` | `FRPG` |
| use | `STW` | `FZ` |
| work | `WR` | `RBG` |
| need | `SKP` | `RPG` |
| remember | `SKW` | `RPL` |
| understand | `SKH` | `RPB` |
| try | `TR` | `RT` |
| expect | `KP` | `BGS` |
| hope | `SWH` | `FPL` |
| hear | `TKP` | `FPG` |
| leave | `TKW` | `FLG` |
| keep | `TKH` | `FBL` |
| learn | `TPW` | `FBG` |
| call | `KHR` | `RL` |
| change | `TWH` | `PBLG` |
| consider | `KPW` | `FRG` |
| love | `HR` | `LG` |
| like | `KWH` | `BLG` |
| seem | `PWH` | `FBLG` |
| imagine | `STKP` | `PLG` |
| care | `STKW` | `FRB` |
| read | `STKH` | `FRP` |
| wish | `STPW` | `FRPL` |
| put | `STPH` | `PT` |
| set | `STWH` | `FT` |
| move | `SKPW` | `PL` |
| live | `SKPH` | `FRLG` |
| remain | `SKWH` | `RPLG` |
| mean | `SPWH` | `PBL` |
| realize | `SR` | `RLG` |
| forget | `TPR` | `RG` |
| mind | `TKPH` | `FPBL` |

IV left-hand anchors:

| Key | Cue |
| --- | --- |
| `H` | have |
| `TK` | initial `d` for do |
| `PW` | initial `b` for be |
| `TKPW` | initial `g` for go |
| `PWHR` | initial `bl` for believe |
| `THR` | tell / `tl` shape |
| `KPL` | make / `k-m` shape |
| `WR` | work / `wr` shape |
| `TPH` | initial `n` for know |
| `TP` | initial `f` for find |
| `TR` | initial `tr` for try |
| `TKPWH` | heavy `g` shape for get |
| `SR` | compact shape for realize |
| `KP` | initial `x` for expect |
| `KHR` | call / hard-c shape |
| `TPR` | forget shape |
| `TKPH` | `d` + `m` for mind |
| `HR` | initial `l` for love |

FV right-hand anchors:

| Key | Cue |
| --- | --- |
| `R` | run |
| `G` | go |
| `B` | be |
| `S` | see |
| `PBT` | want / final `nt` shape |
| `PBG` | think / final `nk` shape |
| `FPB` | find / `f` + `n` |
| `BGS` | expect / x shape |
| `LT` | tell / final `l_t` |
| `RT` | try / `tr` shape |
| `FZ` | use / final `z` sound |
| `RL` | call / compact l-ish shape |
| `PBLG` | change / final `j` |
| `PT` | put / `p_t` |
| `FT` | set / final `st` |

FV verb tiers:

| Tier | Verbs |
| --- | --- |
| strong mnemonic | have, see, do, go, be, want, know, think, look, believe, run, find, tell, try, expect, use, call, change, put, set |
| acceptable shape | say, ask, happen, feel, come, get, become, make, take, give, work, need, remember, understand, hope, hear, leave, keep, learn |
| review / possible overflow | consider, love, like, seem, imagine, care, read, wish, move, live, remain, mean, realize, forget, mind |

## Tail Bank

Used by `IV`. Tail strokes may use the whole right hand except `D`, which is
reserved for past/conditional. Prefer mnemonic or geometric shapes over
preserving an artificial `TSDZ` sub-bank.

| Key | Tail | Cue |
| --- | --- | --- |
| empty | no tail | empty |
| `T` | the | first letter |
| `B` | a | article shape |
| `PB` | an | final `n` shape for `an` |
| `P` | it | short object key |
| `RT` | that | `the` plus deictic shape |
| `TS` | this | `th-s` outline shape |
| `SZ` | these | plural `this` shape |
| `TZ` | those | `th-z` outline shape |
| `PL` | me | final `m` shape |
| `RP` | you | rising U-like shape |
| `R` | your | `r` in `your` |
| `S` | us | final `s` |
| `FR` | her | `r` object shape |
| `FL` | him | object-family shape |
| `RB` | she | compact pronoun shape |
| `RBL` | she will | `she` plus `will` |
| `RBLT` | she'll | contracted `she will` variant |
| `RPB` | he | `P` marks the shorter pronoun branch |
| `RPBL` | he will | `he` plus `will` |
| `RPBLT` | he'll | contracted `he will` variant |
| `GT` | going to | compact `go to` shape |
| `G` | give | first/final strong consonant |
| `BGT` | why | geometric shape analogous to left-hand `KWH` for `Y` |
| `RPL` | who | rising question-tone shape |
| `BLG` | what | broad geometric question shape; distinct from `BGT` why |
| `PBG` | when | final `n` shape plus hook; time-question cue |
| `RLG` | where | curling/location-like shape |
| `PLG` | how | rounded/open geometric question shape |
| `PLT` | them | `th` plus final `m` shape |
| `L` | all | `l` sound |
| `PBT` | one | final `n` plus light variant |

When `D` appears with an `IV` tail, it is the past/conditional grammar key, not
part of the tail code.

## Initial Verb Samples

| Stroke | Output |
| --- | --- |
| `IV[PW-BD]` | was a |
| `IV[PW-PBD]` | was an |
| `IV[PWHR-TD]` | believed the |
| `IV[PW-P]` | is it |
| `IV[S-PD]` | saw it |
| `IV[TKPW*]` | doesn't go |
| `IV[SA-P]` | can see it |
| `IV[SA-PD]` | could see it |
| `IV[SU-P]` | has seen it |
| `IV[TP-PD]` | found it |
| `IV[SKH-FRD]` | understood her |
| `IV[TK-RP]` | do you |
| `IV[S-SZ]` | see these |

## Final Verb Samples

| Stroke | Output |
| --- | --- |
| `FV[K-B]` | he is |
| `FV[T-B]` | they are |
| `FV[S-B]` | I am |
| `FV[T-GD]` | they went |
| `FV[S-FPBD]` | I found |
| `FV[H-PBG]` | you think |
| `FV[SK-BLD]` | she believed |
| `FV[KA-S]` | he can see |
| `FV[KA-SD]` | he could see |
| `FV[KEU-G]` | he has been going |

## Right-Hand Ergonomic Guard

| Shape | Status |
| --- | --- |
| `D` in a phrase stroke | reserved for past/conditional |
| `IV` tail-only shapes without `D` | allowed |
| `IV` tail shapes containing `D` | unassigned |
| right-hand verb plus `D` | allowed as past/conditional |
| mixed right-hand verb plus one `T S Z` key | allowed by review |
| mixed right-hand verb plus two or more `T S Z` keys | banned |

Allowed review examples:

| Shape | Note |
| --- | --- |
| `BD` | one main column plus one tail key |
| `PBT` | one main column plus one tail key |
| `FRZ` | one main column plus one tail key |
| `LGS` | one main column plus one tail key |
| `PB` | easy tail-only chord |

## Non-Verb Families

Used by `NV`. First-tier targets are briefs not covered by standard dictionaries.

| Family key | Family |
| --- | --- |
| empty | pronoun/else overflow |
| `S` | quantity gaps |
| `T` | partitive gaps |
| `K` | subordinator gaps |
| `P` | preposition/function gaps |
| `W` | wh/else gaps |
| `H` | fixed function gaps |

### `NV` Pronoun/Else Overflow

| Key | Phrase |
| --- | --- |
| `F` | anything else |
| `R` | something else |
| `P` | everybody else |
| `B` | everyone else |
| `L` | everything else |
| `G` | nothing else |
| `FR` | no one else |

### `NV` Partitive Gaps

| Key | Phrase |
| --- | --- |
| `T-F` | each of the |
| `T-R` | both of the |
| `T-P` | one of them |
| `T-B` | some of them |
| `T-L` | any of them |
| `T-G` | all of them |

### `NV` Subordinator Gaps

| Key | Phrase |
| --- | --- |
| `K-F` | as if |
| `K-R` | as though |
| `K-P` | even if |
| `K-B` | even when |
| `K-L` | even though |
| `K-G` | assuming that |
| `K-FR` | provided that |
| `K-FP` | except that |
| `K-FB` | in case |
| `K-FL` | because of that |

### `NV` Preposition/Function Gaps

| Key | Phrase |
| --- | --- |
| `P-F` | in that |
| `P-R` | in order to |
| `P-P` | so as to |
| `P-B` | instead of |
| `P-L` | not only |
| `P-G` | not yet |
| `P-FR` | up to |
| `P-FP` | as to whether |

### `NV` Samples

| Stroke | Output |
| --- | --- |
| `NV[T-F]` | each of the |
| `NV[T-P]` | one of them |
| `NV[K-L]` | even though |
| `NV[K-FL]` | because of that |
| `NV[P-R]` | in order to |
| `NV[P-B]` | instead of |
| `NV[-F]` | anything else |

## No-Pedal Follow-Ons

Exact outlines in this table were free in the local standard dictionaries during the draft check.

| Stroke | Operation | Sample |
| --- | --- | --- |
| `*-F` | insert perfect/have | `he goes` -> `he has gone` |
| `*-R` | insert progressive/be | `he goes` -> `he is going` |
| `*-P` | polarity pair | `can` -> `can or cannot` |
| `*-B` | force contraction | `he is` -> `he's` |
| `*-L` | force long form | `he's` -> `he is` |
| `*-G` | informal register | `got to` -> `gotta` |
| `*-T` | insert `just` | `he didn't go` -> `he just didn't go` |
| `*-S` | insert `still` | `he didn't go` -> `he still didn't go` |
| `*-D` | emphatic do-support | `he goes` -> `he does go` |
| `*-Z` | insert `even` | `he can't go` -> `he can't even go` |

## Standard-Dictionary-Covered Non-Verb Examples

Leave these to standard dictionaries unless a longer generated phrase needs them as a component.

| Phrase |
| --- |
| in the |
| in which |
| of the |
| to the |
| for the |
| on the |
| with the |
| one of the |
| some of the |
| any of the |
| all of the |
| rather than |
| other than |
| as well as |
