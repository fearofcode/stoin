# stoin-dict-augment

`stoin-dict-augment` creates a small augmentation dictionary containing safe,
shorter variants of outlines in one or more source dictionaries. The output
contains additions only, so it can be loaded after the source dictionaries.

The tool merges right-hand suffix strokes made from `R`, `L`, `G`, `T`, `S`,
`D`, and `Z` into the preceding stroke whenever none of the required keys are
already occupied. A suffix `G` is added directly when possible. If `G` is
already occupied, `DZ` is used for the `-ing` suffix when both keys are free.
`DZ` is also tried when the ordinary `G` fold is already assigned to a
different source translation, as in `STAB/-G` becoming `STABDZ` because
`STABG` means "stack".
A following stroke made from vowels and a right-hand consonant coda, optionally
preceded by an exact `KWR` linker, can drop its linker and vowels and fold the
complete coda into the preceding stroke when all of its keys are free. For
example, `AEUR/AEUGZ` becomes `AEURGZ`. A result is still omitted when its
canonical chord is already assigned: `AEUP/KWRAER` would become `AEURP`, which
Phoenix already defines as "airplane".
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
A leading consonant-vowel stroke can similarly collapse into the following
vowel stroke. Its vowels are omitted, its left-hand consonants are retained,
and a complete right-hand coda moves to its Lapwing left-hand equivalent. The
moved keys must remain in order and cannot overlap the following stroke. Thus
both `TOR/EPBLGS` and `TU/REPBLGS` collapse to `TREPBLGS`.
Trailing strokes may be dropped from outlines longer than three strokes. The
closure repeats this until three strokes remain, matching Lapwing's conservative
floor. Competing translations for the same shortened prefix make it ambiguous
and therefore omit it.

All compatible adjacent merges, vowel-coda and linker folds, bridge
redistributions, consonant-vowel collapses, vowel-stroke omissions, and
trailing-stroke drops are explored, including changes made possible by earlier
variations. For example, both `STPHUG/-LG` and `STPHUG/-L/-G` can produce
`STPHULGDZ`. Generated outlines use Stoin's canonical stroke spelling.

Existing source outlines are never replaced. A generated outline is omitted if
different source translations claim it, or if it fails the word-boundary
conflict check adapted from `lapwing_augmentor`. A translation is excluded from
augmentation entirely when any of its source outlines contains a standalone
`R-R` stroke after the first stroke; Phoenix uses that stroke to disambiguate
homophones, so shortening those entries is likely to erase the distinction.

```sh
go run ./cmd/stoin-dict-augment \
  -output /path/to/augmentations.json \
  /path/to/source.json [/path/to/another-source.json ...]
```
