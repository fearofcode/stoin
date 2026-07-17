# stoin-dict-augment

`stoin-dict-augment` creates a small augmentation dictionary containing safe,
shorter variants of outlines in one or more source dictionaries. The output
contains additions only, so it can be loaded after the source dictionaries. A
repeatable `-additional` option can also copy non-conflicting entries from a
supplemental dictionary without allowing it to override or augment the primary
sources. A repeatable `-prefer` option can use an existing dictionary, such as
Lapwing, to create or resolve a two-way conflict at an exact generated outline
without making unrelated entries in that dictionary seed augmentations.

The tool merges right-hand suffix strokes made from `R`, `L`, `G`, `T`, `S`,
`D`, and `Z` into the preceding stroke whenever none of the required keys are
already occupied. A suffix `G` is added directly when possible. If `G` is
already occupied, `DZ` is used for the `-ing` suffix when both keys are free.
`DZ` is also tried when the ordinary `G` fold is already assigned to a
different source translation, as in `STAB/-G` becoming `STABDZ` because
`STABG` means "stack".
An outline whose final stroke ends in exact `-GZ` can also gain `-S` before
the `Z`, so `OEGZ` ("ocean") produces `OEGSZ` ("oceans"). This experimental
plural rule is limited to plain translations ending in `n`, whose plural is a
simple appended `s`. It therefore does not affect ordinary `-Z` outlines such
as `RAEUZ` ("raise"), existing plurals, or Plover command translations.
A following stroke made from vowels and a right-hand consonant coda, optionally
preceded by an exact `KWR` linker, can drop its linker and vowels and fold the
complete coda into the preceding stroke when all of its keys are free and both
strokes have exactly the same vowel bank. A narrow plural exception allows an
exact `AEZ` stroke to add `-Z` when the preceding stroke's complete right-hand
coda is `-PS`, so `HREUPS/AEZ` becomes `HREUPSZ`. Other mismatched vowels may
not be dropped: `TPHAOEU/AEBL` does not become `TPHAOEUBL`, `SES/AEBL` does
not collapse "accessibly" toward "accessible(s)", and `SRORS/AEZ` does not
collapse "divorcees" toward "divorces". A vowelless or starred preceding
stroke is also rejected. A result is still omitted when its canonical chord is
already assigned.
An interior stroke consisting of exact `KWR` plus vowels and no right-hand coda
may be omitted completely. For example, `TKEU/PHEUPB/KWRU/T-FB` becomes
the Stoin-canonical `TKEU/PHEUPB/TFB`. A first or final `KWR`-vowel stroke is
not removed.
A leading stroke made only from vowel-bank keys can be omitted. Existing source
outlines still win, so `E/HREUPS` cannot replace `HREUPS` ("lips"), while
`E/HREUPS/AEZ` can become `HREUPS/AEZ` and then `HREUPSZ`.
An internal `AOU` stroke may also be omitted, for example
`ABG/AOU/PH-PB` becomes `ABG/PH-PB`.
An exact `KWR` linker followed by any right-hand suffix chord can be removed
while its suffix keys fold into the preceding stroke. For example,
`PHU/TPHEUP/KWR-L` becomes the canonical outline `PHU/TPHEUPL`.
For outlines with at least three strokes, a limited vowel-less bridge can also
be redistributed: left `S`, `T`, `P`, or `R` moves to the matching right-hand
key on the preceding vowel stroke, while a lone right `-L` becomes left `HR-`
on the following vowel stroke. Thus `PAPB/T-L/AOUPB` becomes the lossless
`PAPBT/HRAOUPB`.
An interior stroke made entirely from a recognized left-hand consonant chord
followed by vowels can move that consonant chord to the preceding stroke and
drop its linker vowels. This is a limited form of Lapwing's alternate syllable
splitting rather than general split generation: for example,
`KUL/TU/SRAEUT` becomes `KULT/SRAEUT`. The complete consonant must have a
Lapwing left-to-right mapping and its destination keys must be free.
For outlines of at least three strokes, a second stroke made only from
left-hand consonants followed by vowels may instead be omitted completely.
This covers Phoenix's redundant medial syllable strokes, for example
`KAUZ/PHO/PAUL/T-PB` becoming `KAUZ/PAUL/T-PB` and `KAUZ/PHU/TAULG`
becoming `KAUZ/TAULG`. The rule applies only to the second stroke and does not
accept a right-hand coda.
A two-stroke outline can similarly collapse completely into one stroke when its
leading consonant-vowel stroke moves into the following vowel stroke. Its
vowels are omitted, its left-hand consonants are retained, and a complete
right-hand coda moves to its Lapwing left-hand equivalent. The moved keys must
remain in order and cannot overlap the following stroke. Thus both
`TOR/EPBLGS` and `TU/REPBLGS` collapse to `TREPBLGS`. Longer outlines are not
partially collapsed; in particular, `HEU/U/HRAOEU` does not become
`HU/HRAOEU`. Unless an exact `-prefer` entry can adjudicate it, a one-stroke
result ending in exact final `-D` is also rejected when removing `-D` reveals
an existing plain source outline whose translation does not share a plausible
stem with the collapsed translation. This prevents `PU/HRAEUD/KWR-PB`
("Palladian") from eventually claiming `PHRAEUPBD` over the `PHRAEUPB`
("plain") family while leaving unrelated final-key collapses such as
`PO/HRAEUR/-S` ("Polaris") eligible.
Trailing strokes may be dropped from outlines longer than three strokes. The
closure repeats this until three strokes remain, matching Lapwing's conservative
floor. Competing translations for the same shortened prefix make it ambiguous
and therefore omit it. A drop is also refused when the shortened outline's
all-but-final prefix and final stroke are separately defined source entries
and the final stroke uses a left-joining Plover suffix command. This prevents
`HU/HRAOUS/TPHAEUT/-FB` ("hallucinative") from shadowing the compositional
`HU/HRAOUS` + `TPHAEUT` ("hallucinate").

All compatible adjacent merges, vowel-coda and linker folds, bridge
redistributions, consonant-vowel collapses, vowel-stroke omissions, and
trailing-stroke drops are explored, including changes made possible by earlier
variations. For example, both `STPHUG/-LG` and `STPHUG/-L/-G` can produce
`STPHULGDZ`. Generated outlines use Stoin's canonical stroke spelling.

Existing source outlines are never replaced. A generated outline is omitted if
different source translations claim it and its final-stroke starred variant is
unavailable, or if it fails the word-boundary conflict check adapted from
`lapwing_augmentor`. When exactly two translations claim an outline and adding
`*` to its final stroke is unoccupied, both are retained. The translation with
the higher count in the bundled `count_1w.txt` corpus receives the ordinary
outline and the other receives both the starred outline and an outline with a
standalone `/R-R` disambiguation stroke appended. If that `/R-R` outline is
already occupied, another `/R-R` stroke is appended until an unused outline is
found. If either translation is absent from the corpus, the longer translation
receives the ordinary outline;
equal counts or lengths are resolved lexically for deterministic output.
When a `-prefer` dictionary defines a generated outline with a different
translation, its entry becomes another claim for that outline. If that produces
exactly two claimed translations, the preferred translation receives the
ordinary outline before corpus frequency or the fallback ranking is considered.
The other translation remains available on the starred and `/R-R` outlines. A
preference that introduces a third translation makes the conflict ambiguous,
and later `-prefer` dictionaries take precedence over earlier ones.
Conflicts with three or more translations remain omitted. A translation is
excluded from augmentation entirely when any of its source outlines contains
a standalone `R-R` stroke after the first stroke; Phoenix uses that stroke to disambiguate
homophones, so shortening those entries is likely to erase the distinction.
Generated outlines ending in the exact standalone `P-P` stroke are also
discarded. Phoenix uses it as a hyphen/join marker whose translation depends on
the stroke that follows, so emitting such a generated outline can prematurely
commit the wrong compound word. Source outlines ending in `P-P` do not seed
augmentation, and generated outlines ending there are not expanded further;
an internal `P-P` with a following completion stroke remains eligible.

Supplemental dictionaries passed with `-additional` have lower authority than
every positional source dictionary. Their entries do not seed folding or other
generation rules. An entry is copied only when its canonical outline does not
already exist in the primary sources or the accepted generated augmentations,
and it survives the same candidate ambiguity, word-boundary, `R-R`, and
trailing-`P-P` safeguards as generated entries. Rejected generated candidates
do not reserve their outlines. This allows an unused Magnum outline such as
`AUBLGS` for "auxiliary" to be imported without replacing a Phoenix definition
or a usable generated entry.

```sh
go run ./cmd/stoin-dict-augment \
  -output /path/to/augmentations.json \
  -prefer /path/to/lapwing-base.json \
  -additional /path/to/magnum.json \
  /path/to/source.json [/path/to/another-source.json ...]
```
