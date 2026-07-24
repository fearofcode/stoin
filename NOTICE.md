# Third-Party Notices

## Project License and Plover-Derived Code

Stoin is free software licensed under the GNU General Public License, version 2,
or (at your option) any later version. See `LICENSE.txt`.

Stoin includes code and data ported, adapted, or developed with reference to
Plover:

<https://github.com/opensteno/plover>

- `src/format.c` and `src/orthography.c` port or adapt Plover's formatting and
  English orthography implementations.
- Gemini PR support in `src/gemini_pr.c`, Stentura support in
  `src/stentura.c`, and TX Bolt support in `src/tx_bolt.c` port or adapt
  Plover's corresponding machine protocol implementations.
- QWERTY steno input and key mapping in `src/steno.c`, `src/keymap.c`, and the
  platform keyboard-capture code were developed with reference to Plover's
  keyboard machine, keymap, and keyboard-capture implementations.
- `american_english_words.txt` is copied from
  `plover/assets/american_english_words.txt`.

Relevant upstream files carry copyright notices for Joshua Harlan Lifton
(2010-2011) and Hesky Fisher (2011); see the Plover source files for the
individual notices.

Plover is distributed under the GNU General Public License, version 2 or (at
your option) any later version. Stoin's ports and adaptations have been
rewritten and modified.

## Norvig Word-Frequency Data

`stoin-dict-augment` downloads Peter Norvig's `count_1w.txt` word-frequency
file on demand from:

<https://www.norvig.com/ngrams/count_1w.txt>

The file is cached locally and is not distributed in this repository. Norvig
states that the data files are derived from the Google Web Trillion Word Corpus,
which was distributed by the Linguistic Data Consortium. His corpus page is:

<https://www.norvig.com/ngrams/>

## Lapwing Dictionaries

The bundled `lapwing-base.json` and `lapwing-commands.json` dictionaries are
derived from `aerickt/plover-lapwing-aio`:

<https://github.com/aerickt/plover-lapwing-aio>

Lapwing AIO is MIT licensed:

```text
MIT License

Copyright (c) 2024 Aerick

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Vendored C Libraries

- `third_party/stb_ds.h` is Sean Barrett's `stb_ds.h`, public domain / MIT licensed; see the header for the full notice.
- `third_party/cjson` is cJSON; see `third_party/cjson/LICENSE` and `third_party/cjson/UPSTREAM`.
