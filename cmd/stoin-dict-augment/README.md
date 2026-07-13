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

All compatible adjacent merges, linker folds, bridge redistributions, and `AOU`
omissions are explored, including changes made possible by earlier variations.
For example, both `STPHUG/-LG` and `STPHUG/-L/-G` can produce `STPHULGDZ`.
Generated outlines use Stoin's canonical stroke spelling.

Existing source outlines are never replaced. A generated outline is omitted if
different source translations claim it, or if it fails the word-boundary
conflict check adapted from `lapwing_augmentor`.

```sh
go run ./cmd/stoin-dict-augment \
  -output /path/to/augmentations.json \
  /path/to/source.json [/path/to/another-source.json ...]
```
